/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 *
 * Cloud Transport — HTTPS + JWT/ECDSA to Google Cloud Pub/Sub.
 */

#pragma once

#include "esp_err.h"
#include "telemetry_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the cloud transport subsystem.
 *
 * Creates the cloud_uplink_task pinned to Core 0.
 * Waits for Ethernet connectivity before starting TLS.
 *
 * @return ESP_OK on success.
 */
esp_err_t cloud_transport_init(void);

/**
 * @brief Check if the cloud transport is connected and authenticated.
 */
bool cloud_transport_is_connected(void);

#ifdef __cplusplus
}
#endif
