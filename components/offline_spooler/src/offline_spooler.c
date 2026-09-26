/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#include "offline_spooler.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "crc16.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "storage_manager.h"

static const char *TAG = "offline_spooler";

#define SPOOL_FILE_PATH "/sdcard/spool/offline_buffer.dat"
#define MAGIC_BYTES     0xEDCE

static uint32_t s_read_cursor = 0;

// Frame structure:
// [Magic: 2B] [Length: 2B] [Payload: N Bytes] [CRC16: 2B]

esp_err_t offline_spooler_init(void)
{
    if (storage_manager_is_degraded()) {
        ESP_LOGW(TAG, "Storage is degraded. Spooler will be disabled.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Initializing Offline Spooler...");

    nvs_handle_t nvs;
    if (nvs_open("spooler", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_get_u32(nvs, "cursor", &s_read_cursor);
        nvs_close(nvs);
    }

    // Ensure the spool directory exists
    mkdir("/sdcard/spool", 0755);

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

esp_err_t offline_spooler_peek(uint32_t in_cursor, uint8_t *out_buffer, uint16_t max_len,
                               uint16_t *out_len, uint32_t *out_next_cursor)
{
    if (storage_manager_is_degraded()) {
        return ESP_FAIL;
    }

    FILE *f = fopen(SPOOL_FILE_PATH, "rb");
    if (f == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    fseek(f, in_cursor, SEEK_SET);

    uint16_t magic = 0;
    if (fread(&magic, 1, sizeof(magic), f) != sizeof(magic)) {
        fclose(f);
        return ESP_ERR_NOT_FOUND;  // EOF
    }

    if (magic != MAGIC_BYTES) {
        fclose(f);
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t len_buf[2];
    if (fread(len_buf, 1, 2, f) != 2) {
        fclose(f);
        return ESP_FAIL;
    }

    uint16_t length = len_buf[0] | (len_buf[1] << 8);
    if (length > max_len) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    if (fread(out_buffer, 1, length, f) != length) {
        fclose(f);
        return ESP_FAIL;
    }

    uint16_t crc = 0;
    if (fread(&crc, 1, sizeof(crc), f) != sizeof(crc)) {
        fclose(f);
        return ESP_FAIL;
    }

    uint8_t crc_calc_buf[2 + length];
    crc_calc_buf[0] = len_buf[0];
    crc_calc_buf[1] = len_buf[1];
    memcpy(&crc_calc_buf[2], out_buffer, length);

    uint16_t expected_crc = crc16_ccitt(crc_calc_buf, sizeof(crc_calc_buf));
    if (crc != expected_crc) {
        ESP_LOGE(TAG, "Spooler CRC mismatch");
        fclose(f);
        return ESP_ERR_INVALID_CRC;
    }

    *out_next_cursor = ftell(f);
    fclose(f);

    *out_len = length;
    return ESP_OK;
}

void offline_spooler_commit(uint32_t new_cursor)
{
    s_read_cursor = new_cursor;
    nvs_handle_t nvs;
    if (nvs_open("spooler", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_u32(nvs, "cursor", s_read_cursor);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

uint32_t offline_spooler_get_cursor(void)
{
    return s_read_cursor;
}

esp_err_t offline_spooler_compact(void)
{
    if (storage_manager_is_degraded())
        return ESP_FAIL;

    // To prevent fragmentation and excessive SD wear, we only compact if read_cursor is large
    // enough
    if (s_read_cursor < 32 * 1024) {
        return ESP_OK;  // No need to compact yet
    }

    ESP_LOGI(TAG, "Compacting spooler. Current cursor: %lu", (unsigned long)s_read_cursor);

    FILE *f_in = fopen(SPOOL_FILE_PATH, "rb");
    if (!f_in)
        return ESP_FAIL;

    FILE *f_out = fopen("/sdcard/offline_spooler.tmp", "wb");
    if (!f_out) {
        fclose(f_in);
        return ESP_FAIL;
    }

    fseek(f_in, s_read_cursor, SEEK_SET);

    uint8_t buf[1024];
    size_t read_bytes;
    while ((read_bytes = fread(buf, 1, sizeof(buf), f_in)) > 0) {
        fwrite(buf, 1, read_bytes, f_out);
    }

    fclose(f_in);
    fclose(f_out);

    // Atomically rename
    remove(SPOOL_FILE_PATH);
    rename("/sdcard/offline_spooler.tmp", SPOOL_FILE_PATH);

    // Reset cursor
    offline_spooler_commit(0);
    ESP_LOGI(TAG, "Compaction complete.");
    return ESP_OK;
}
