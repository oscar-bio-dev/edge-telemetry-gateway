/*
 * SPDX-FileCopyrightText: 2026 EcoTech Sensing
 * SPDX-License-Identifier: Apache-2.0
 *
 * IPC Sender — COBS encode + UART TX towards ESP32-P4 Host.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "ipc_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize UART for IPC transmission to the P4 host.
 */
esp_err_t ipc_sender_init(void);

/**
 * @brief Send a COBS-encoded IPC frame to the P4 host.
 *
 * @param type      Message type.
 * @param src_mac   Source MAC of the ESP-NOW node (6 bytes).
 * @param rssi      RSSI of the received packet.
 * @param payload   Raw payload data (Protobuf).
 * @param payload_len Length of payload.
 *
 * @return ESP_OK on success.
 */
esp_err_t ipc_sender_send_frame(ipc_msg_type_t type,
                                const uint8_t *src_mac,
                                int8_t rssi,
                                const uint8_t *payload,
                                size_t payload_len);

#ifdef __cplusplus
}
#endif
