/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the Spooler subsystem.
 * Opens or creates the offline_buffer.dat file, validating existing data.
 */
esp_err_t offline_spooler_init(void);

/**
 * @brief Appends a raw Protobuf payload to the spooler.
 * Automatically wraps the payload in the Magic Bytes, Length, and CRC framing.
 *
 * @param pb_data Pointer to the serialized Protobuf.
 * @param length Length of the serialized data.
 * @return ESP_OK on success, ESP_FAIL on disk error.
 */
esp_err_t offline_spooler_append(const uint8_t *pb_data, uint16_t length);

/**
 * @brief Reads the next item from the spooler WITHOUT advancing the persistent cursor.
 *
 * @param in_cursor The cursor position to read from (usually s_read_cursor).
 * @param out_buffer Pre-allocated buffer to hold the read Protobuf payload.
 * @param max_len Maximum length of out_buffer.
 * @param out_len Actual length of the payload popped.
 * @param out_next_cursor The cursor position immediately after this item.
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if empty or end of file reached.
 */
esp_err_t offline_spooler_peek(uint32_t in_cursor, uint8_t *out_buffer, uint16_t max_len,
                               uint16_t *out_len, uint32_t *out_next_cursor);

/**
 * @brief Returns the current committed read cursor position.
 */
uint32_t offline_spooler_get_cursor(void);

/**
 * @brief Commits the read cursor to NVS, marking items as durable-delivered.
 *
 * @param new_cursor The cursor position returned by peek().
 */
void offline_spooler_commit(uint32_t new_cursor);

/**
 * @brief Compacts the SD spool file to free up space (rotates the file if cursor is advanced).
 * @return ESP_OK on success.
 */
esp_err_t offline_spooler_compact(void);

#ifdef __cplusplus
}
#endif
