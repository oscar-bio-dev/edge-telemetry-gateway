/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#include "jwt_generator.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "mbedtls/base64.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/pk.h"
#include "mbedtls/sha256.h"

static const char *TAG = "jwt_gen";

#ifndef CONFIG_HW_ECDSA_ENABLE
// The development key embedded via target_add_binary_data
extern const uint8_t dev_private_key_pem_start[] asm("_binary_dev_private_key_pem_start");
extern const uint8_t dev_private_key_pem_end[] asm("_binary_dev_private_key_pem_end");
#endif

esp_err_t jwt_generate_es256(const char *project_id, int validity_minutes, char *out_buffer,
                             size_t buffer_len)
{
    // 1. Generate JWT Header and Payload (Base64url encoded)
    // 2. Hash with SHA-256
    // 3. Sign using ECDSA (ES256)

    time_t now = time(NULL);
    time_t exp = now + (validity_minutes * 60);

    // Mocking the Unsigned JWT Base64 for the skeleton
    char unsigned_jwt[256];
    snprintf(unsigned_jwt, sizeof(unsigned_jwt),
             "eyJhbGciOiJFUzI1NiIsInR5cCI6IkpXVCJ9."
             "eyJpYXQiOiVsbGQsImV4cCI6JWxsZCwiYXVkIjoiJXNifQ",
             (long long)now, (long long)exp, project_id);

    unsigned char hash[32];
    mbedtls_sha256_context sha_ctx;
    mbedtls_sha256_init(&sha_ctx);
    mbedtls_sha256_starts(&sha_ctx, 0);  // 0 = SHA-256
    mbedtls_sha256_update(&sha_ctx, (const unsigned char *)unsigned_jwt, strlen(unsigned_jwt));
    mbedtls_sha256_finish(&sha_ctx, hash);
    mbedtls_sha256_free(&sha_ctx);

    unsigned char signature[64] = {0};  // r and s are 32 bytes each for P-256
    size_t sig_len = 0;

#ifdef CONFIG_HW_ECDSA_ENABLE
    ESP_LOGI(TAG, "Signing JWT using Hardware ECDSA_DS peripheral (eFuse key)...");

    // NOTE: In a real implementation, you would use esp_ecdsa_sign() or
    // construct a specific mbedTLS context that hooks into the hardware.
    // For this skeleton, we assume the hardware API call here:
    // esp_err_t err = esp_ecdsa_sign_hash(hash, signature, &sig_len);
    // if (err != ESP_OK) return err;

#else
    ESP_LOGI(TAG, "Signing JWT using Software mbedTLS (Development Key)...");

    mbedtls_pk_context pk;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    mbedtls_pk_init(&pk);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    const char *pers = "jwt_gen";
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *)pers,
                          strlen(pers));

    size_t key_len = dev_private_key_pem_end - dev_private_key_pem_start;

    int ret = mbedtls_pk_parse_key(&pk, dev_private_key_pem_start, key_len, NULL, 0,
                                   mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret == 0) {
        // Sign the hash (Note: mbedtls_ecdsa_write_signature outputs ASN.1 DER which needs to be
        // converted to raw R/S for JWT) Here we just use a stub for the skeleton
        sig_len = 64;
    } else {
        ESP_LOGE(TAG, "Failed to parse dev private key: -0x%04x", -ret);
    }

    mbedtls_pk_free(&pk);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    if (ret != 0)
        return ESP_FAIL;
#endif

    // Append signature to JWT (Base64url encoded)
    snprintf(out_buffer, buffer_len, "%s.MOCK_SIGNATURE_BASE64", unsigned_jwt);

    return ESP_OK;
}
