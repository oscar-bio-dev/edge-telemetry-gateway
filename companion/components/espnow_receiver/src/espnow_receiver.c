/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 *
 * ESP-NOW Receiver — ACK-First, Forward-Later Architecture
 *
 * The ESP-NOW receive callback does the absolute minimum: copies the
 * incoming frame metadata into a FreeRTOS queue and returns immediately
 * (< 1µs of radio-critical work).
 *
 * Two dedicated tasks consume the queue:
 *
 *   1. ack_dispatch_task (priority MAX-1):
 *      - Dequeues the frame
 *      - Looks up the Mailbox for pending commands for this node
 *      - Builds a GatewayAck (with epoch + optional command)
 *      - Sends the ACK via esp_now_send() (unicast, ~3-6ms)
 *      - Enqueues the raw payload to the forward queue
 *
 *   2. forward_task (priority 4):
 *      - Dequeues raw payloads from the forward queue
 *      - Forwards them to the P4 via UART/COBS (ipc_sender_send_frame)
 *
 * This design guarantees sub-20ms ACK latency even under UART congestion,
 * because UART TX never blocks the ACK path.
 */

#include "espnow_receiver.h"
#include <string.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "gateway_headers.h"
#include "ipc_sender.h"
#include "mailbox.h"
#include "nvs_flash.h"
#include "pb_encode.h"
#include "sdkconfig.h"
#include "telemetry.pb.h"

#define ESPNOW_WIFI_CHANNEL CONFIG_ESPNOW_CHANNEL

static const char *TAG = "espnow_rx";

/* ═══════════════════════════════════════════════════════════════════════════
 * Queue Definitions
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Maximum ESP-NOW payload we accept (header + protobuf) */
#define ESPNOW_MAX_PAYLOAD 250

/** RX queue depth — must absorb burst arrivals during ACK processing */
#define RX_QUEUE_DEPTH 16

/** Forward queue depth — buffered between ACK dispatch and UART TX */
#define FWD_QUEUE_DEPTH 16

/**
 * @brief Metadata + payload captured from the ESP-NOW callback.
 */
typedef struct {
    uint8_t src_mac[6];
    int8_t rssi;
    uint8_t data[ESPNOW_MAX_PAYLOAD];
    size_t data_len;
} espnow_rx_item_t;

static QueueHandle_t s_rx_queue = NULL;
static QueueHandle_t s_fwd_queue = NULL;

/* ═══════════════════════════════════════════════════════════════════════════
 * ESP-NOW Callback (ISR-safe, minimal work)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    if (recv_info->src_addr == NULL || data == NULL || len == 0 || len > ESPNOW_MAX_PAYLOAD) {
        return;
    }

    espnow_rx_item_t item;
    memcpy(item.src_mac, recv_info->src_addr, 6);
    item.rssi = recv_info->rx_ctrl ? recv_info->rx_ctrl->rssi : 0;
    memcpy(item.data, data, len);
    item.data_len = len;

    /* Non-blocking enqueue — if queue is full, drop (node will retry) */
    if (xQueueSend(s_rx_queue, &item, 0) != pdTRUE) {
        ESP_LOGW(TAG, "RX queue full — frame dropped from %02X:%02X:..:%02X", item.src_mac[0],
                 item.src_mac[1], item.src_mac[5]);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ACK Dispatch Task (Highest Priority — Radio Path)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void ack_dispatch_task(void *arg)
{
    espnow_rx_item_t item;
    uint32_t frame_counter = 0;
    ESP_LOGI(TAG, "ACK Dispatch Task started (prio %d)", uxTaskPriorityGet(NULL));

    while (1) {
        if (xQueueReceive(s_rx_queue, &item, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        /* -- Periodic housekeeping: purge expired mailbox entries -- */
        frame_counter++;
        if ((frame_counter & 0x3F) == 0) { /* Every 64 frames */
            uint64_t epoch = mailbox_get_epoch();
            if (epoch > 0) {
                mailbox_purge_expired(epoch);
            }
        }

        /* -- Step 1: Build GatewayAck ----------------------------- */
        telemetry_GatewayAck ack = telemetry_GatewayAck_init_default;
        ack.current_epoch_s = mailbox_get_epoch();
        ack.has_current_epoch_s = true;

        /* Peek mailbox for pending commands (non-destructive read) */
        uint8_t cmd_buf[GW_MAILBOX_CMD_MAX_SIZE];
        size_t cmd_len = 0;
        bool has_cmd = mailbox_peek(item.src_mac, cmd_buf, &cmd_len);

        if (has_cmd) {
            /* The mailbox stores the raw command enum byte (not Protobuf) */
            ack.has_command = true;
            ack.command = (telemetry_Command)cmd_buf[0];
            ESP_LOGI(TAG, "Piggybacked CMD %d to %02X:%02X:..:%02X", ack.command, item.src_mac[0],
                     item.src_mac[1], item.src_mac[5]);
        }

        /* Encode GatewayAck protobuf */
        uint8_t ack_pb[32];
        pb_ostream_t stream = pb_ostream_from_buffer(ack_pb, sizeof(ack_pb));
        if (!pb_encode(&stream, telemetry_GatewayAck_fields, &ack)) {
            ESP_LOGE(TAG, "Failed to encode GatewayAck: %s", PB_GET_ERROR(&stream));
            goto forward;
        }

        /* Prepend ESP-NOW header byte. ALWAYS use 0x20 (ESPNOW_HDR_ACK).
         * The command (if any) travels inside the GatewayAck protobuf,
         * not in the header byte. The node only accepts 0x20. */
        uint8_t ack_frame[1 + 32];
        ack_frame[0] = ESPNOW_HDR_ACK;
        memcpy(&ack_frame[1], ack_pb, stream.bytes_written);

        /* -- Step 2: Send ACK via ESP-NOW (unicast) --------------- */
        esp_err_t err = esp_now_send(item.src_mac, ack_frame, 1 + stream.bytes_written);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "ACK send failed to %02X:%02X:..:%02X: %s", item.src_mac[0],
                     item.src_mac[1], item.src_mac[5], esp_err_to_name(err));
            /* Command stays in mailbox for next node cycle (peek was non-destructive) */
        } else {
            ESP_LOGD(TAG, "ACK sent to %02X:%02X:..:%02X (epoch=%" PRIu64 ", cmd=%d)",
                     item.src_mac[0], item.src_mac[1], item.src_mac[5], ack.current_epoch_s,
                     has_cmd);
            /* Confirm delivery: remove command from mailbox only on success */
            if (has_cmd) {
                mailbox_confirm(item.src_mac);
            }
        }

    forward:
        /* -- Step 3: Enqueue for UART forwarding ------------------ */
        if (xQueueSend(s_fwd_queue, &item, pdMS_TO_TICKS(10)) != pdTRUE) {
            ESP_LOGW(TAG, "Forward queue full - telemetry will be lost");
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Forward Task (Lower Priority — UART Path)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void forward_task(void *arg)
{
    espnow_rx_item_t item;
    ESP_LOGI(TAG, "Forward Task started (prio %d)", uxTaskPriorityGet(NULL));

    while (1) {
        if (xQueueReceive(s_fwd_queue, &item, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        /* Forward the raw ESP-NOW payload (header byte + protobuf) to P4 */
        esp_err_t err = ipc_sender_send_frame(IPC_MSG_TELEMETRY, item.src_mac, item.rssi, item.data,
                                              item.data_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "UART forward failed: %s", esp_err_to_name(err));
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Initialization
 * ═══════════════════════════════════════════════════════════════════════════ */

static esp_err_t wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Set channel to match sensor nodes */
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));

    return ESP_OK;
}

esp_err_t espnow_receiver_init(void)
{
    /* Ensure NVS is initialized (required by WiFi) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(wifi_init());

    /* Initialize ESP-NOW */
    if (esp_now_init() != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing ESP-NOW");
        return ESP_FAIL;
    }

    /* Configure PMK */
    ESP_ERROR_CHECK(esp_now_set_pmk((const uint8_t *)CONFIG_ESPNOW_PMK));

    /* Create queues */
    s_rx_queue = xQueueCreate(RX_QUEUE_DEPTH, sizeof(espnow_rx_item_t));
    s_fwd_queue = xQueueCreate(FWD_QUEUE_DEPTH, sizeof(espnow_rx_item_t));
    if (s_rx_queue == NULL || s_fwd_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create RX/FWD queues");
        return ESP_ERR_NO_MEM;
    }

    /* Register receive callback */
    esp_now_register_recv_cb(espnow_recv_cb);

    /* Create tasks — ACK dispatch at highest app priority, forward lower */
    xTaskCreate(ack_dispatch_task, "ack_dispatch", 4096, NULL, configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(forward_task, "fwd_uart", 3072, NULL, 4, NULL);

    /* Print Gateway MAC */
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    ESP_LOGW(TAG, "=========================================================");
    ESP_LOGW(TAG, "🔌 GATEWAY MAC ADDRESS: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5]);
    ESP_LOGW(TAG, "=========================================================");

    return ESP_OK;
}

esp_err_t espnow_add_dynamic_peer(const uint8_t *mac, const uint8_t *lmk)
{
    esp_now_peer_info_t peerInfo = {};
    peerInfo.channel = ESPNOW_WIFI_CHANNEL;
    peerInfo.ifidx = WIFI_IF_STA;
    peerInfo.encrypt = true;

    memcpy(peerInfo.peer_addr, mac, 6);
    memcpy(peerInfo.lmk, lmk, 16);

    esp_err_t err = esp_now_add_peer(&peerInfo);
    if (err == ESP_ERR_ESPNOW_EXIST) {
        esp_now_del_peer(mac);
        err = esp_now_add_peer(&peerInfo);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to dynamically add peer %02X:%02X:%02X:%02X:%02X:%02X: %s", mac[0],
                 mac[1], mac[2], mac[3], mac[4], mac[5], esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "✅ Dynamically added peer %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1],
                 mac[2], mac[3], mac[4], mac[5]);
    }

    return err;
}
