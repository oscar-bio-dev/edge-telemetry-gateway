/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

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
 * @brief Reads up to `max_items` from the spooler for flushing.
 * Note: For simplicity in this implementation, this reads the entire payload. 
 * A full implementation would manage a read-pointer cursor.
 * 
 * @param out_buffer Pre-allocated array of pointers to hold the read Protobufs.
 * @param max_items Max number of items to pop.
 * @param out_count Actual number of items popped.
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if empty.
 */
esp_err_t offline_spooler_pop(uint8_t **out_buffer, uint16_t max_items, uint16_t *out_count);

#ifdef __cplusplus
}
#endif
