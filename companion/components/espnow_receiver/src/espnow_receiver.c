/*
 * SPDX-FileCopyrightText: 2026 EcoTech Sensing
 * SPDX-License-Identifier: Apache-2.0
 */

#include "espnow_receiver.h"
#include "esp_log.h"

static const char *TAG = "espnow_receiver";

esp_err_t espnow_receiver_init(void)
{
    ESP_LOGI(TAG, "ESP-NOW receiver initialized (stub)");
    return ESP_OK;
}
