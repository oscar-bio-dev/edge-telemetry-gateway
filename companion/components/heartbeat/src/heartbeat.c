/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#include "heartbeat.h"
#include "esp_log.h"

static const char *TAG = "heartbeat";

esp_err_t heartbeat_init(void)
{
    ESP_LOGI(TAG, "Heartbeat timer initialized (stub)");
    return ESP_OK;
}
