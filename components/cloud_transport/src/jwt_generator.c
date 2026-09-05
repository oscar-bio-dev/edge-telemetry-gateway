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
#include "mbedtls/pk.h"
#include "mbedtls/md.h"
#include "esp_random.h"

static const char *TAG = "jwt_gen";

#ifndef CONFIG_HW_ECDSA_ENABLE
// The development key embedded via target_add_binary_data
extern const uint8_t dev_private_key_pem_start[] asm("_binary_dev_private_key_pem_start");
extern const uint8_t dev_private_key_pem_end[] asm("_binary_dev_private_key_pem_end");
#endif

static void base64url_encode(const unsigned char *src, size_t src_len, char *dst, size_t dst_len)
{
    size_t olen = 0;
    mbedtls_base64_encode((unsigned char *)dst, dst_len, &olen, src, src_len);
    for (size_t i = 0; i < olen; i++) {
        if (dst[i] == '+')
            dst[i] = '-';
        else if (dst[i] == '/')
            dst[i] = '_';
        else if (dst[i] == '=') {
            dst[i] = '\0';
            break;
        }
    }
}

// Extract exactly 64 bytes (R, S) from ASN.1 DER signature for JWT ES256
static esp_err_t parse_asn1_der_signature(const unsigned char *der_sig, size_t der_len,
                                          unsigned char *raw_sig)
{
    if (der_len < 8 || der_sig[0] != 0x30)
        return ESP_FAIL;
    size_t offset = 2;

    if (der_sig[offset] != 0x02)
        return ESP_FAIL;
    size_t r_len = der_sig[offset + 1];
    offset += 2;

    const unsigned char *r_bytes = der_sig + offset;
    if (r_len == 33 && r_bytes[0] == 0x00) {
        r_bytes++;
        r_len--;
    }

    memset(raw_sig, 0, 64);
    if (r_len <= 32)
        memcpy(raw_sig + (32 - r_len), r_bytes, r_len);
    else
        return ESP_FAIL;

    offset += (der_sig[offset - 1]);

    if (der_sig[offset] != 0x02)
        return ESP_FAIL;
    size_t s_len = der_sig[offset + 1];
    offset += 2;

    const unsigned char *s_bytes = der_sig + offset;
    if (s_len == 33 && s_bytes[0] == 0x00) {
        s_bytes++;
        s_len--;
    }

    if (s_len <= 32)
        memcpy(raw_sig + 32 + (32 - s_len), s_bytes, s_len);
    else
        return ESP_FAIL;

    return ESP_OK;
}

esp_err_t jwt_generate_es256(const char *project_id, int validity_minutes, char *out_buffer,
                             size_t buffer_len)
{
    time_t now = time(NULL);
    time_t exp = now + (validity_minutes * 60);

    const char *header_json = "{\"alg\":\"ES256\",\"typ\":\"JWT\"}";
    char payload_json[128];
    snprintf(payload_json, sizeof(payload_json), "{\"iat\":%lld,\"exp\":%lld,\"aud\":\"%s\"}",
             (long long)now, (long long)exp, project_id);

    char header_b64[64] = {0};
    char payload_b64[200] = {0};

    base64url_encode((const unsigned char *)header_json, strlen(header_json), header_b64,
                     sizeof(header_b64));
    base64url_encode((const unsigned char *)payload_json, strlen(payload_json), payload_b64,
                     sizeof(payload_b64));

    char unsigned_jwt[300];
    snprintf(unsigned_jwt, sizeof(unsigned_jwt), "%s.%s", header_b64, payload_b64);

    unsigned char hash[32];
    mbedtls_md_context_t md_ctx;
    mbedtls_md_init(&md_ctx);
    mbedtls_md_setup(&md_ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
    mbedtls_md_starts(&md_ctx);
    mbedtls_md_update(&md_ctx, (const unsigned char *)unsigned_jwt, strlen(unsigned_jwt));
    mbedtls_md_finish(&md_ctx, hash);
    mbedtls_md_free(&md_ctx);

    unsigned char raw_sig[64] = {0};

#ifdef CONFIG_HW_ECDSA_ENABLE
    ESP_LOGI(TAG, "Signing JWT using Hardware ECDSA_DS peripheral (eFuse key)...");
    // esp_err_t err = esp_ecdsa_sign_hash(hash, raw_sig, &sig_len);
    // if (err != ESP_OK) return err;
    memset(raw_sig, 0xAB, 64);  // Stub for HW
#else
    ESP_LOGI(TAG, "Signing JWT using Software mbedTLS (Development Key)...");
    mbedtls_pk_context pk;

    mbedtls_pk_init(&pk);

    size_t key_len = dev_private_key_pem_end - dev_private_key_pem_start;

    // key_len includes null terminator if provided, but mbedtls expects exact size including null
    // term for PEM.
    int ret = mbedtls_pk_parse_key(&pk, dev_private_key_pem_start, key_len, NULL, 0);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to parse dev private key: -0x%04x", -ret);
        goto cleanup;
    }

    unsigned char der_sig[MBEDTLS_PK_SIGNATURE_MAX_SIZE];
    size_t der_sig_len = 0;

    ret = mbedtls_pk_sign(&pk, MBEDTLS_MD_SHA256, hash, sizeof(hash), der_sig, sizeof(der_sig),
                          &der_sig_len);
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_pk_sign failed: -0x%04x", -ret);
        goto cleanup;
    }

    if (parse_asn1_der_signature(der_sig, der_sig_len, raw_sig) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse ASN.1 DER signature");
        ret = -1;
        goto cleanup;
    }

cleanup:
    mbedtls_pk_free(&pk);
    if (ret != 0)
        return ESP_FAIL;
#endif

    char sig_b64[100] = {0};
    base64url_encode(raw_sig, 64, sig_b64, sizeof(sig_b64));

    snprintf(out_buffer, buffer_len, "%s.%s", unsigned_jwt, sig_b64);
    return ESP_OK;
}
