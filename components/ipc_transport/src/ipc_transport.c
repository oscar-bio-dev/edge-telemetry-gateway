/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ipc_transport.h"
#include "cobs.h"
#include "crc16.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ipc_frame.h"
#include "sdkconfig.h"
#include "telemetry_buffer.h"
#include "telemetry_decoder.h"

#define UART_PORT_NUM    UART_NUM_1
#define UART_BAUD_RATE   CONFIG_IPC_UART_BAUD_RATE
#define UART_TX_PIN      CONFIG_IPC_UART_TX_GPIO
#define UART_RX_PIN      CONFIG_IPC_UART_RX_GPIO
#define UART_RX_BUF_SIZE 2048

static const char *TAG = "ipc_transport";

static void ipc_ingest_task(void *arg)
{
    uint8_t rx_buffer[1024];                           // Raw UART bytes
    uint8_t frame_buffer[IPC_ENCODED_FRAME_MAX_SIZE];  // Single COBS frame accumulator
    size_t frame_idx = 0;

    ESP_LOGI(TAG, "IPC Ingest Task started on Core 1");

    while (1) {
        int rx_bytes =
            uart_read_bytes(UART_PORT_NUM, rx_buffer, sizeof(rx_buffer), pdMS_TO_TICKS(100));

        for (int i = 0; i < rx_bytes; i++) {
            uint8_t b = rx_buffer[i];

            if (b == 0x00) {
                // End of frame delimiter
                if (frame_idx > 0) {
                    // Process frame
                    uint8_t decoded_buf[IPC_RAW_FRAME_MAX_SIZE];
                    size_t decoded_len = cobs_decode(frame_buffer, frame_idx, decoded_buf);

                    if (decoded_len >= IPC_FRAME_MIN_SIZE) {
                        // Validate CRC
                        uint16_t expected_crc =
                            (decoded_buf[decoded_len - 2] << 8) | decoded_buf[decoded_len - 1];
                        uint16_t calc_crc = crc16_ccitt(decoded_buf, decoded_len - 2);

                        if (calc_crc == expected_crc) {
                            ipc_header_t *header = (ipc_header_t *)decoded_buf;

                            // Decode Protobuf payload
                            telemetry_TelemetryPayload payload_struct;
                            size_t pb_len = decoded_len - sizeof(ipc_header_t) - 2;
                            const uint8_t *pb_data = decoded_buf + sizeof(ipc_header_t);

                            esp_err_t decode_err = telemetry_decode_payload(
                                pb_data, pb_len, header->src_mac, &payload_struct);
                            if (decode_err == ESP_OK) {
                                // Push to Ring Buffer
                                esp_err_t push_err = telemetry_buffer_push(&payload_struct);
                                if (push_err == ESP_OK) {
                                    ESP_LOGD(TAG, "Telemetry pushed to buffer. MAC: %s",
                                             payload_struct.device_id);
                                } else {
                                    ESP_LOGW(TAG, "Telemetry dropped. Buffer full.");
                                }
                            } else {
                                ESP_LOGE(TAG, "Failed to decode telemetry payload");
                            }

                        } else {
                            ESP_LOGW(TAG, "CRC Error: Calc 0x%04X != Exp 0x%04X", calc_crc,
                                     expected_crc);
                        }
                    } else {
                        // 0 length or too small (e.g. malformed or consecutive 0x00)
                        if (decoded_len > 0) {
                            ESP_LOGW(TAG, "Decoded frame too small: %d", (int)decoded_len);
                        }
                    }
                    frame_idx = 0;  // Reset for next frame
                }
            } else {
                if (frame_idx < sizeof(frame_buffer)) {
                    frame_buffer[frame_idx++] = b;
                } else {
                    ESP_LOGE(TAG, "Frame buffer overflow! Dropping.");
                    frame_idx = 0;
                }
            }
        }
    }
}

esp_err_t ipc_transport_init(void)
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

    err = uart_driver_install(UART_PORT_NUM, UART_RX_BUF_SIZE, 0, 0, NULL, 0);
    if (err != ESP_OK)
        return err;

    // Pin task to Core 1 (Ingest / App Core)
    xTaskCreatePinnedToCore(ipc_ingest_task, "ipc_ingest", 4096, NULL, 5, NULL, 1);

    ESP_LOGI(TAG, "UART IPC Initialized on TX:%d RX:%d @ %d bps", UART_TX_PIN, UART_RX_PIN,
             UART_BAUD_RATE);
    return ESP_OK;
}

esp_err_t ipc_transport_send(uint8_t type, const uint8_t *data, size_t len)
{
    if (len > IPC_PAYLOAD_MAX_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t raw_frame[IPC_RAW_FRAME_MAX_SIZE];
    ipc_header_t *header = (ipc_header_t *)raw_frame;

    // Fill header
    header->type = type;
    memset(header->src_mac, 0, 6);  // P4 is the sender, doesn't need to spoof a MAC
    header->seq_num = 0;            // Optional: increment sequence
    header->rssi = 0;               // N/A for Host -> C6

    // Copy payload
    if (data && len > 0) {
        memcpy(raw_frame + sizeof(ipc_header_t), data, len);
    }

    // Calculate CRC16
    size_t data_len = sizeof(ipc_header_t) + len;
    uint16_t crc = crc16_ccitt(raw_frame, data_len);
    raw_frame[data_len] = (crc >> 8) & 0xFF;
    raw_frame[data_len + 1] = crc & 0xFF;

    size_t frame_len = data_len + 2;

    // COBS encode
    uint8_t encoded_frame[IPC_ENCODED_FRAME_MAX_SIZE];
    encoded_frame[0] = 0x00;  // Start sentinel
    size_t encoded_len = cobs_encode(raw_frame, frame_len, encoded_frame + 1);
    encoded_frame[1 + encoded_len] = 0x00;  // End sentinel

    // Send over UART
    int tx_bytes = uart_write_bytes(UART_PORT_NUM, encoded_frame, encoded_len + 2);
    if (tx_bytes != (encoded_len + 2)) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t ipc_transport_send_add_peer(const uint8_t *mac, const uint8_t *lmk)
{
    ipc_add_peer_payload_t payload;
    memcpy(payload.mac, mac, 6);
    memcpy(payload.lmk, lmk, 16);

    ESP_LOGI(TAG, "Sending IPC_MSG_ADD_PEER to C6 for MAC %02X:%02X:%02X:%02X:%02X:%02X", mac[0],
             mac[1], mac[2], mac[3], mac[4], mac[5]);

    return ipc_transport_send(IPC_MSG_ADD_PEER, (const uint8_t *)&payload, sizeof(payload));
}
