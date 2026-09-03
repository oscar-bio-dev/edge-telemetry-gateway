/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 *
 * Telemetry Buffer — Static ring buffer for decoded samples.
 * Zero-allocation design. Uses FreeRTOS queue internally.
 */

#pragma once

#include "esp_err.h"
#include "telemetry_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t telemetry_buffer_init(void);
esp_err_t telemetry_buffer_push(const decoded_telemetry_t *sample);
size_t    telemetry_buffer_pop_batch(decoded_telemetry_t *out, size_t max_count,
                                    uint32_t timeout_ms);
size_t    telemetry_buffer_count(void);

#ifdef __cplusplus
}
#endif
