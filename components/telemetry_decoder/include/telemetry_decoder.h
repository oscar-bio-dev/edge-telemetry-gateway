/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "telemetry.pb.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the telemetry decoder component.
 */
esp_err_t telemetry_decoder_init(void);

/**
 * @brief Decode raw Protobuf bytes into a C struct and inject the device identity.
 *
 * @param[in]  raw_pb      Pointer to raw Protobuf data from UART/COBS.
 * @param[in]  len         Length of raw_pb in bytes.
 * @param[in]  src_mac     Source MAC address (6 bytes) to format into device_id.
 *                         If NULL, device_id will be empty.
 * @param[out] out_data    Pointer to output struct to populate.
 *
 * @return ESP_OK on success, or an error code on deserialization failure.
 */
esp_err_t telemetry_decode_payload(const uint8_t *raw_pb, size_t len, const uint8_t *src_mac,
                                   telemetry_TelemetryPayload *out_data);

#ifdef __cplusplus
}
#endif
