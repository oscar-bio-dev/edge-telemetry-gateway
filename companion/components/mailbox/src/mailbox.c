/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mailbox.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "mailbox";

static mailbox_entry_t s_entries[GW_MAILBOX_MAX_ENTRIES];
static SemaphoreHandle_t s_mutex = NULL;

/** Default TTL for commands: 5 minutes */
#define MAILBOX_TTL_SECONDS 300

/** Cached epoch from P4 (updated via IPC_HDR_SYNC_EPOCH) */
static uint64_t s_current_epoch = 0;

void mailbox_set_epoch(uint64_t epoch)
{
    s_current_epoch = epoch;
}

uint64_t mailbox_get_epoch(void)
{
    return s_current_epoch;
}

esp_err_t mailbox_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    memset(s_entries, 0, sizeof(s_entries));
    ESP_LOGI(TAG, "Mailbox initialized (%d slots, %d bytes/cmd, TTL=%ds)", GW_MAILBOX_MAX_ENTRIES,
             GW_MAILBOX_CMD_MAX_SIZE, MAILBOX_TTL_SECONDS);
    return ESP_OK;
}

esp_err_t mailbox_put(const uint8_t *mac, const uint8_t *payload, size_t payload_len)
{
    if (payload_len > GW_MAILBOX_CMD_MAX_SIZE) {
        return ESP_ERR_INVALID_SIZE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    /* First pass: look for existing entry with same MAC (overwrite) */
    for (int i = 0; i < GW_MAILBOX_MAX_ENTRIES; i++) {
        if (s_entries[i].occupied && memcmp(s_entries[i].mac, mac, 6) == 0) {
            memcpy(s_entries[i].payload, payload, payload_len);
            s_entries[i].payload_len = payload_len;
            s_entries[i].expire_epoch = s_current_epoch + MAILBOX_TTL_SECONDS;
            xSemaphoreGive(s_mutex);
            ESP_LOGD(TAG, "Overwritten entry for %02X:%02X:..:%02X", mac[0], mac[1], mac[5]);
            return ESP_OK;
        }
    }

    /* Second pass: find a free slot */
    for (int i = 0; i < GW_MAILBOX_MAX_ENTRIES; i++) {
        if (!s_entries[i].occupied) {
            memcpy(s_entries[i].mac, mac, 6);
            memcpy(s_entries[i].payload, payload, payload_len);
            s_entries[i].payload_len = payload_len;
            s_entries[i].expire_epoch = s_current_epoch + MAILBOX_TTL_SECONDS;
            s_entries[i].occupied = true;
            xSemaphoreGive(s_mutex);
            ESP_LOGI(TAG, "Stored command for %02X:%02X:%02X:%02X:%02X:%02X (slot %d)", mac[0],
                     mac[1], mac[2], mac[3], mac[4], mac[5], i);
            return ESP_OK;
        }
    }

    xSemaphoreGive(s_mutex);
    ESP_LOGW(TAG, "Mailbox FULL — command for %02X:%02X:..:%02X dropped!", mac[0], mac[1], mac[5]);
    return ESP_ERR_NO_MEM;
}

bool mailbox_take(const uint8_t *mac, uint8_t *out_payload, size_t *out_len)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    for (int i = 0; i < GW_MAILBOX_MAX_ENTRIES; i++) {
        if (s_entries[i].occupied && memcmp(s_entries[i].mac, mac, 6) == 0) {
            /* Check expiration */
            if (s_current_epoch > 0 && s_entries[i].expire_epoch < s_current_epoch) {
                s_entries[i].occupied = false;
                xSemaphoreGive(s_mutex);
                ESP_LOGD(TAG, "Expired entry for %02X:%02X:..:%02X", mac[0], mac[1], mac[5]);
                return false;
            }
            memcpy(out_payload, s_entries[i].payload, s_entries[i].payload_len);
            *out_len = s_entries[i].payload_len;
            s_entries[i].occupied = false;
            xSemaphoreGive(s_mutex);
            return true;
        }
    }

    xSemaphoreGive(s_mutex);
    return false;
}

int mailbox_purge_expired(uint64_t now_epoch)
{
    int purged = 0;
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    for (int i = 0; i < GW_MAILBOX_MAX_ENTRIES; i++) {
        if (s_entries[i].occupied && s_entries[i].expire_epoch < now_epoch) {
            s_entries[i].occupied = false;
            purged++;
        }
    }

    xSemaphoreGive(s_mutex);
    if (purged > 0) {
        ESP_LOGI(TAG, "Purged %d expired entries", purged);
    }
    return purged;
}
