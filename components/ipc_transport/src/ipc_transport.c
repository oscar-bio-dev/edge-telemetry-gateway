/*
 * SPDX-FileCopyrightText: 2026 EcoTech Sensing
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ipc_transport.h"
#include "esp_log.h"

static const char *TAG = "ipc_transport";

esp_err_t ipc_transport_init(void)
{
    ESP_LOGI(TAG, "%s initialized (stub)", TAG);
    return ESP_OK;
}
