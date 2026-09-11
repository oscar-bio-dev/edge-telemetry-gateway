/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ipc_sender.h"
#include <string.h>
#include "cobs.h"
#include "crc16.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "espnow_receiver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "ipc_frame.h"

#define UART_PORT_NUM  UART_NUM_1
#define UART_BAUD_RATE CONFIG_IPC_UART_BAUD_RATE
#define UART_TX_PIN    CONFIG_IPC_UART_TX_GPIO
#define UART_RX_PIN    CONFIG_IPC_UART_RX_GPIO
#define UART_BUF_SIZE  1024

static const char *TAG = "ipc_sender";
static uint16_t global_seq_num = 0;
static QueueHandle_t uart_evt_que = NULL;

static void ipc_rx_task(void *arg)
{
    uart_event_t event;
    uint8_t *dtmp = (uint8_t *)malloc(UART_BUF_SIZE);

    uint8_t frame_buffer[IPC_ENCODED_FRAME_MAX_SIZE];
    size_t frame_idx = 0;

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
                                    ipc_header_t *header = (ipc_header_t *)decoded_buf;
                                    if (header->type == IPC_MSG_ADD_PEER) {
                                        ipc_add_peer_payload_t *payload =
                                            (ipc_add_peer_payload_t *)(decoded_buf +
                                                                       sizeof(ipc_header_t));
                                        espnow_add_dynamic_peer(payload->mac, payload->lmk);
                                    }
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

    // Copy payload
    if (payload && payload_len > 0) {
        memcpy(raw_frame + sizeof(ipc_header_t), payload, payload_len);
    }

    size_t raw_len = sizeof(ipc_header_t) + payload_len;

    // Calculate CRC16
    uint16_t crc = crc16_ccitt(raw_frame, raw_len);
    raw_frame[raw_len++] = (uint8_t)(crc >> 8);
    raw_frame[raw_len++] = (uint8_t)(crc & 0xFF);

    // Encode with COBS
    uint8_t encoded_frame[IPC_ENCODED_FRAME_MAX_SIZE];
    encoded_frame[0] = 0x00;  // Leading delimiter

    size_t encoded_len = cobs_encode(raw_frame, raw_len, &encoded_frame[1]);
    if (encoded_len == 0) {
        ESP_LOGE(TAG, "COBS encoding failed");
        return ESP_FAIL;
    }

    encoded_frame[1 + encoded_len] = 0x00;  // Trailing delimiter

    size_t total_tx_len = 2 + encoded_len;

    // Send over UART
    int tx_bytes = uart_write_bytes(UART_PORT_NUM, encoded_frame, total_tx_len);
    if (tx_bytes != total_tx_len) {
        ESP_LOGE(TAG, "UART TX failed: sent %d of %zu", tx_bytes, total_tx_len);
        return ESP_FAIL;
    }

    // Wait until TX is done to ensure the payload is actually out (optional, but good for stability
    // if bursts are rare)
    uart_wait_tx_done(UART_PORT_NUM, pdMS_TO_TICKS(50));

    return ESP_OK;
}
