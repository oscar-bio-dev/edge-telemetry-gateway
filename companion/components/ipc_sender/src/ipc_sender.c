/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ipc_sender.h"
#include "esp_log.h"

static const char *TAG = "ipc_sender";

esp_err_t ipc_sender_init(void)
{
    ESP_LOGI(TAG, "IPC sender initialized (stub)");
    return ESP_OK;
}

esp_err_t ipc_sender_send_frame(ipc_msg_type_t type,
                                const uint8_t *src_mac,
                                int8_t rssi,
                                const uint8_t *payload,
                                size_t payload_len)
{
    ESP_LOGI(TAG, "Frame TX: type=0x%02X len=%zu (stub)", type, payload_len);
    return ESP_OK;
}
