/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#include "telemetry_buffer.h"
#include "esp_log.h"

static const char *TAG = "telemetry_buffer";

esp_err_t telemetry_buffer_init(void)
{
    ESP_LOGI(TAG, "%s initialized (stub)", TAG);
    return ESP_OK;
}
