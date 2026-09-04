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
 * @brief Initialize the telemetry buffer (FreeRTOS Queue in RAM).
 */
esp_err_t telemetry_buffer_init(void);

/**
 * @brief Push a new decoded telemetry payload into the buffer.
 *
 * @param payload Pointer to the decoded telemetry struct.
 * @return ESP_OK if queued successfully, ESP_ERR_NO_MEM if queue is full.
 */
esp_err_t telemetry_buffer_push(const telemetry_TelemetryPayload *payload);

/**
 * @brief Pop up to `max_items` from the buffer into the provided array.
 *
 * @param out_array Array to store the popped payloads.
 * @param max_items Maximum number of items to pop.
 * @return Number of items actually popped.
 */
size_t telemetry_buffer_pop_batch(telemetry_TelemetryPayload *out_array, size_t max_items);

#ifdef __cplusplus
}
#endif
