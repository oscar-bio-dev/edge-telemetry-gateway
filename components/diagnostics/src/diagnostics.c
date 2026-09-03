/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#include "diagnostics.h"
#include "esp_log.h"

static const char *TAG = "diagnostics";

esp_err_t diagnostics_init(void)
{
    ESP_LOGI(TAG, "%s initialized (stub)", TAG);
    return ESP_OK;
}
