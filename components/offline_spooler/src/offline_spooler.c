/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#include "offline_spooler.h"
#include <stdio.h>
#include <string.h>
#include "crc16.h"
#include "esp_log.h"
#include "storage_manager.h"

static const char *TAG = "offline_spooler";

#define SPOOL_FILE_PATH "/sdcard/spool/offline_buffer.dat"
#define MAGIC_BYTES     0xEDCE

// Frame structure:
// [Magic: 2B] [Length: 2B] [Payload: N Bytes] [CRC16: 2B]

esp_err_t offline_spooler_init(void)
{
    if (storage_manager_is_degraded()) {
        ESP_LOGW(TAG, "Storage is degraded. Spooler will be disabled.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Initializing Offline Spooler...");

    // Attempt to open the file to ensure the path exists and is writable
    FILE *f = fopen(SPOOL_FILE_PATH, "ab");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open spool file for appending.");
        return ESP_FAIL;
    }

    // Could do a quick scan to validate tail frame or count items, but for now we just verify
    // access
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);

    ESP_LOGI(TAG, "Spool file ready. Current size: %ld bytes", size);

    return ESP_OK;
}

esp_err_t offline_spooler_append(const uint8_t *pb_data, uint16_t length)
{
    if (storage_manager_is_degraded()) {
        return ESP_FAIL;
    }

    FILE *f = fopen(SPOOL_FILE_PATH, "ab");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open spool file during append.");
        return ESP_FAIL;
    }

    uint16_t magic = MAGIC_BYTES;

    // CRC is computed over [Length (2B)] + [Payload (N Bytes)]
    uint8_t crc_buf[2 + length];
    crc_buf[0] = length & 0xFF;
    crc_buf[1] = (length >> 8) & 0xFF;
    memcpy(&crc_buf[2], pb_data, length);

    uint16_t crc = crc16_ccitt(crc_buf, sizeof(crc_buf));

    // Write Frame
    size_t written = 0;
    written += fwrite(&magic, 1, sizeof(magic), f);
    written += fwrite(&crc_buf[0], 1, 2, f);  // Length (little endian)
    written += fwrite(pb_data, 1, length, f);
    written += fwrite(&crc, 1, sizeof(crc), f);

    fclose(f);

    if (written != (2 + 2 + length + 2)) {
        ESP_LOGE(TAG, "Failed to write complete frame to disk.");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Appended %d bytes payload to offline spooler.", length);
    return ESP_OK;
}

esp_err_t offline_spooler_pop(uint8_t **out_buffer, uint16_t max_items, uint16_t *out_count)
{
    if (storage_manager_is_degraded()) {
        return ESP_FAIL;
    }

    // Implementation of pop requires read cursors and compaction/truncation strategies.
    // For this Phase, we are focusing on Append-Only integrity, so popping is a stub.

    *out_count = 0;
    return ESP_ERR_NOT_FOUND;
}
