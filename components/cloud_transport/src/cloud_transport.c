/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cloud_transport.h"
#include <stdbool.h>
#include "esp_log.h"

static const char *TAG = "cloud_transport";

esp_err_t cloud_transport_init(void)
{
    ESP_LOGI(TAG, "Cloud transport initialized (stub)");
    return ESP_OK;
}

bool cloud_transport_is_connected(void)
{
    return false;
}
