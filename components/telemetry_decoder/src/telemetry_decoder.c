/*
 * SPDX-FileCopyrightText: 2026 EcoTech Sensing
 * SPDX-License-Identifier: Apache-2.0
 */

#include "telemetry_decoder.h"
#include "esp_log.h"

static const char *TAG = "telemetry_decoder";

esp_err_t telemetry_decoder_init(void)
{
    ESP_LOGI(TAG, "%s initialized (stub)", TAG);
    return ESP_OK;
}
