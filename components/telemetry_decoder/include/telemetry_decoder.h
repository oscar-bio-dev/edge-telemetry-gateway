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

/**
 * @brief Decodes a raw Protobuf byte array into a DiagnosticReport struct.
 *
 * @param raw_pb Pointer to the Protobuf byte array
 * @param len Length of the array
 * @param out_data Pointer to the struct to populate
 * @return esp_err_t ESP_OK on success, ESP_FAIL on decode error
 */
esp_err_t telemetry_decode_diagnostic(const uint8_t *raw_pb, size_t len,
                                      telemetry_DiagnosticReport *out_data);

#ifdef __cplusplus
}
#endif
