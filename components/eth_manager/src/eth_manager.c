/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#include "eth_manager.h"
#include "esp_log.h"

static const char *TAG = "eth_manager";

esp_err_t eth_manager_init(void)
{
    ESP_LOGI(TAG, "%s initialized (stub)", TAG);
    return ESP_OK;
}
