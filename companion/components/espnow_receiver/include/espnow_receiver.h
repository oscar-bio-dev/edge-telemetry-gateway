/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 *
 * ESP-NOW Receiver — Listens for broadcast telemetry from sensor nodes.
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize WiFi in STA mode and register ESP-NOW receive callback.
 *
 * Configures WiFi channel from Kconfig and registers the ESP-NOW
 * receive callback that forwards data to the IPC sender.
 *
 * @return ESP_OK on success.
 */
esp_err_t espnow_receiver_init(void);

/**
 * @brief Dynamically add a new ESP-NOW peer to the Wi-Fi RAM.
 *
 * @param mac 6-byte MAC address
 * @param lmk 16-byte Local Master Key
 * @return esp_err_t
 */
esp_err_t espnow_add_dynamic_peer(const uint8_t *mac, const uint8_t *lmk);

#ifdef __cplusplus
}
#endif
