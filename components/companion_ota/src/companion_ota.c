/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#include "companion_ota.h"
#include "esp_log.h"

static const char *TAG = "companion_ota";

esp_err_t companion_ota_flash(const uint8_t *firmware, size_t firmware_size)
{
    ESP_LOGI(TAG, "OTA flash requested (%zu bytes) — stub", firmware_size);
    return ESP_ERR_NOT_SUPPORTED;
}
