/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 *
 * IPC Sender — Bidirectional UART bridge between C6 and P4.
 *
 * TX path: Encodes IPC frames (COBS + CRC16) and sends to P4.
 * RX path: Decodes incoming IPC frames from P4 and routes them:
 *   - IPC_MSG_ADD_PEER  → espnow_add_dynamic_peer()
 *   - IPC_HDR_SYNC_EPOCH → mailbox_set_epoch()
 *   - IPC_HDR_CMD_INJECT → mailbox_put()
 */

#include "ipc_sender.h"
#include <string.h>
#include "cobs.h"
#include "crc16.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "gateway_headers.h"
#include "ipc_frame.h"
#include "sdkconfig.h"

#include "espnow_receiver.h"
#include "mailbox.h"

#define UART_PORT_NUM  UART_NUM_1
#define UART_BAUD_RATE CONFIG_IPC_UART_BAUD_RATE
#define UART_TX_PIN    CONFIG_IPC_UART_TX_GPIO
#define UART_RX_PIN    CONFIG_IPC_UART_RX_GPIO
#define UART_BUF_SIZE  1024

static const char *TAG = "ipc_sender";
static uint16_t global_seq_num = 0;
static QueueHandle_t uart_evt_que = NULL;

/* ═══════════════════════════════════════════════════════════════════════════
 * IPC RX Router — Process commands from P4
 * ═══════════════════════════════════════════════════════════════════════════ */

static void route_ipc_frame(const uint8_t *decoded_buf, size_t decoded_len)
{
    ipc_header_t *header = (ipc_header_t *)decoded_buf;
    const uint8_t *payload = decoded_buf + sizeof(ipc_header_t);
    size_t payload_len = decoded_len - sizeof(ipc_header_t) - 2; /* minus CRC16 */

    switch (header->type) {
        case IPC_MSG_ADD_PEER: {
            if (payload_len >= sizeof(ipc_add_peer_payload_t)) {
                ipc_add_peer_payload_t *peer = (ipc_add_peer_payload_t *)payload;
                espnow_add_dynamic_peer(peer->mac, peer->lmk);
            } else {
                ESP_LOGW(TAG, "ADD_PEER payload too short: %zu", payload_len);
            }
            break;
        }

        case IPC_HDR_SYNC_EPOCH: {
            if (payload_len >= sizeof(uint64_t)) {
                uint64_t epoch;
                memcpy(&epoch, payload, sizeof(uint64_t));
                mailbox_set_epoch(epoch);
                ESP_LOGI(TAG, "Epoch synchronized: %" PRIu64, epoch);
            } else {
                ESP_LOGW(TAG, "SYNC_EPOCH payload too short: %zu", payload_len);
            }
            break;
        }

        case IPC_HDR_CMD_INJECT: {
            if (payload_len >= 7) { /* MAC(6) + at least 1 byte of command */
                const uint8_t *target_mac = payload;
                const uint8_t *cmd_data = payload + 6;
                size_t cmd_len = payload_len - 6;
                esp_err_t err = mailbox_put(target_mac, cmd_data, cmd_len);
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "CMD injected for %02X:%02X:%02X:%02X:%02X:%02X (%zu bytes)",
                             target_mac[0], target_mac[1], target_mac[2], target_mac[3],
                             target_mac[4], target_mac[5], cmd_len);
                }
            } else {
                ESP_LOGW(TAG, "CMD_INJECT payload too short: %zu", payload_len);
            }
            break;
        }

        default:
            ESP_LOGW(TAG, "Unknown IPC type from P4: 0x%02X", header->type);
            break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * IPC RX Task — COBS decode loop for frames from P4
 * ═══════════════════════════════════════════════════════════════════════════ */

static void ipc_rx_task(void *arg)
{
    uart_event_t event;
    uint8_t *dtmp = (uint8_t *)malloc(UART_BUF_SIZE);

    uint8_t frame_buffer[IPC_ENCODED_FRAME_MAX_SIZE];
    size_t frame_idx = 0;

    ESP_LOGI(TAG, "IPC RX Task started");

    while (1) {
        if (xQueueReceive(uart_evt_que, (void *)&event, portMAX_DELAY)) {
            if (event.type == UART_DATA) {
                int rx_bytes = uart_read_bytes(UART_PORT_NUM, dtmp, event.size, portMAX_DELAY);
                for (int i = 0; i < rx_bytes; i++) {
                    uint8_t b = dtmp[i];
                    if (b == 0x00) {
                        if (frame_idx > 0) {
                            uint8_t decoded_buf[IPC_RAW_FRAME_MAX_SIZE];
                            size_t decoded_len = cobs_decode(frame_buffer, frame_idx, decoded_buf);
                            if (decoded_len >= IPC_FRAME_MIN_SIZE) {
                                uint16_t expected_crc = (decoded_buf[decoded_len - 2] << 8) |
                                                        decoded_buf[decoded_len - 1];
                                uint16_t calc_crc = crc16_ccitt(decoded_buf, decoded_len - 2);
                                if (calc_crc == expected_crc) {
                                    route_ipc_frame(decoded_buf, decoded_len);
                                } else {
                                    ESP_LOGW(TAG, "CRC Error in RX: Calc 0x%04X != Exp 0x%04X",
                                             calc_crc, expected_crc);
                                }
                            }
                            frame_idx = 0;
                        }
                    } else {
                        if (frame_idx < sizeof(frame_buffer)) {
                            frame_buffer[frame_idx++] = b;
                        } else {
                            ESP_LOGE(TAG, "RX Frame buffer overflow! Dropping.");
                            frame_idx = 0;
                        }
                    }
                }
            } else if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL) {
                uart_flush_input(UART_PORT_NUM);
                xQueueReset(uart_evt_que);
            }
        }
    }
    free(dtmp);
    vTaskDelete(NULL);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Initialization & TX API
 * ═══════════════════════════════════════════════════════════════════════════ */

esp_err_t ipc_sender_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_param_config(UART_PORT_NUM, &uart_config);
    if (err != ESP_OK)
        return err;

    err = uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE);
    if (err != ESP_OK)
        return err;

    err = uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE, UART_BUF_SIZE, 20, &uart_evt_que, 0);
    if (err != ESP_OK)
        return err;

    xTaskCreate(ipc_rx_task, "ipc_rx", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "UART IPC Initialized on TX:%d RX:%d @ %d bps", UART_TX_PIN, UART_RX_PIN,
             UART_BAUD_RATE);
    return ESP_OK;
}

esp_err_t ipc_sender_send_frame(ipc_msg_type_t type, const uint8_t *src_mac, int8_t rssi,
                                const uint8_t *payload, size_t payload_len)
{
    if (payload_len > IPC_PAYLOAD_MAX_SIZE) {
        ESP_LOGE(TAG, "Payload too large: %zu > %d", payload_len, IPC_PAYLOAD_MAX_SIZE);
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t raw_frame[IPC_RAW_FRAME_MAX_SIZE];
    ipc_header_t *header = (ipc_header_t *)raw_frame;

    header->type = type;
    if (src_mac) {
        memcpy(header->src_mac, src_mac, 6);
    } else {
        memset(header->src_mac, 0, 6);
    }
    header->seq_num = global_seq_num++;
    header->rssi = rssi;

    /* Copy payload */
    if (payload && payload_len > 0) {
        memcpy(raw_frame + sizeof(ipc_header_t), payload, payload_len);
    }

    size_t raw_len = sizeof(ipc_header_t) + payload_len;

    /* Calculate CRC16 */
    uint16_t crc = crc16_ccitt(raw_frame, raw_len);
    raw_frame[raw_len++] = (uint8_t)(crc >> 8);
    raw_frame[raw_len++] = (uint8_t)(crc & 0xFF);

    /* Encode with COBS */
    uint8_t encoded_frame[IPC_ENCODED_FRAME_MAX_SIZE];
    encoded_frame[0] = 0x00; /* Leading delimiter */

    size_t encoded_len = cobs_encode(raw_frame, raw_len, &encoded_frame[1]);
    if (encoded_len == 0) {
        ESP_LOGE(TAG, "COBS encoding failed");
        return ESP_FAIL;
    }

    encoded_frame[1 + encoded_len] = 0x00; /* Trailing delimiter */

    size_t total_tx_len = 2 + encoded_len;

    /* Send over UART */
    int tx_bytes = uart_write_bytes(UART_PORT_NUM, encoded_frame, total_tx_len);
    if (tx_bytes != total_tx_len) {
        ESP_LOGE(TAG, "UART TX failed: sent %d of %zu", tx_bytes, total_tx_len);
        return ESP_FAIL;
    }

    /* Wait until TX is done */
    uart_wait_tx_done(UART_PORT_NUM, pdMS_TO_TICKS(50));

    return ESP_OK;
}
