/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ipc_sender.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "cobs.h"
#include "crc16.h"
#include "sdkconfig.h"
#include <string.h>

#define UART_PORT_NUM      UART_NUM_1
#define UART_BAUD_RATE     CONFIG_IPC_UART_BAUD_RATE
#define UART_TX_PIN        CONFIG_IPC_UART_TX_GPIO
#define UART_RX_PIN        CONFIG_IPC_UART_RX_GPIO
#define UART_BUF_SIZE      1024

static const char *TAG = "ipc_sender";
static uint16_t global_seq_num = 0;

esp_err_t ipc_sender_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_param_config(UART_PORT_NUM, &uart_config);
    if (err != ESP_OK) return err;

    err = uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;

    err = uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE, UART_BUF_SIZE, 0, NULL, 0);
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "UART IPC Sender Initialized on TX:%d RX:%d @ %d bps", UART_TX_PIN, UART_RX_PIN, UART_BAUD_RATE);
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
    encoded_frame[0] = 0x00; // Leading delimiter
    
    size_t encoded_len = cobs_encode(raw_frame, raw_len, &encoded_frame[1]);
    if (encoded_len == 0) {
        ESP_LOGE(TAG, "COBS encoding failed");
        return ESP_FAIL;
    }

    encoded_frame[1 + encoded_len] = 0x00; // Trailing delimiter
    
    size_t total_tx_len = 2 + encoded_len;
    
    // Send over UART
    int tx_bytes = uart_write_bytes(UART_PORT_NUM, encoded_frame, total_tx_len);
    if (tx_bytes != total_tx_len) {
        ESP_LOGE(TAG, "UART TX failed: sent %d of %zu", tx_bytes, total_tx_len);
        return ESP_FAIL;
    }

    // Wait until TX is done to ensure the payload is actually out (optional, but good for stability if bursts are rare)
    uart_wait_tx_done(UART_PORT_NUM, pdMS_TO_TICKS(50));

    return ESP_OK;
}
