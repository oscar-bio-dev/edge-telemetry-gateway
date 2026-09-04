/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include "esp_err.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Generates an ES256 JWT for Google Cloud Pub/Sub
 * 
 * Uses hardware ECDSA if CONFIG_HW_ECDSA_ENABLE is set,
 * otherwise falls back to software mbedTLS using the embedded dev_private_key.pem.
 * 
 * @param project_id The GCP Project ID (used as audience)
 * @param validity_minutes How long the JWT is valid for
 * @param out_buffer Buffer to write the JWT string
 * @param buffer_len Size of the output buffer
 * @return ESP_OK on success
 */
esp_err_t jwt_generate_es256(const char *project_id, int validity_minutes, char *out_buffer, size_t buffer_len);

#ifdef __cplusplus
}
#endif
