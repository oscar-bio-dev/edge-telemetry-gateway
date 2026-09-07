/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#include "telemetry_decoder.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_random.h"
#include "pb_decode.h"

static const char *TAG = "telemetry_decoder";

esp_err_t telemetry_decoder_init(void)
{
    ESP_LOGI(TAG, "Telemetry decoder initialized");
    return ESP_OK;
}

esp_err_t telemetry_decode_payload(const uint8_t *raw_pb, size_t len, const uint8_t *src_mac,
                                   telemetry_TelemetryPayload *out_data)
{
    if (raw_pb == NULL || out_data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // Zero-initialize the structure
    *out_data = (telemetry_TelemetryPayload)telemetry_TelemetryPayload_init_zero;

    // Create Nanopb stream
    pb_istream_t stream = pb_istream_from_buffer(raw_pb, len);

    // Decode the Protobuf buffer
    bool status = pb_decode(&stream, telemetry_TelemetryPayload_fields, out_data);
    if (!status) {
        ESP_LOGE(TAG, "Protobuf decoding failed: %s", PB_GET_ERROR(&stream));
        return ESP_FAIL;
    }

    // Inject the Device ID string using the MAC address if provided.
    // E.g., "sensor-aa:bb:cc:dd:ee:ff"
    if (src_mac != NULL) {
        snprintf(out_data->device_id, sizeof(out_data->device_id),
                 "sensor-%02X:%02X:%02X:%02X:%02X:%02X", src_mac[0], src_mac[1], src_mac[2],
                 src_mac[3], src_mac[4], src_mac[5]);
    }

    // Inject unique event_id (UUID v4) to guarantee backend idempotency
    uint8_t rnd[16];
    esp_fill_random(rnd, sizeof(rnd));
    rnd[6] = (rnd[6] & 0x0f) | 0x40;  // Version 4
    rnd[8] = (rnd[8] & 0x3f) | 0x80;  // Variant 1
    snprintf(out_data->event_id, sizeof(out_data->event_id),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", rnd[0], rnd[1],
             rnd[2], rnd[3], rnd[4], rnd[5], rnd[6], rnd[7], rnd[8], rnd[9], rnd[10], rnd[11],
             rnd[12], rnd[13], rnd[14], rnd[15]);

    // Note: The timestamp is left empty (0) as per architectural decision,
    // since we do not have an SNTP synchronized RTC on the P4 yet, and
    // GCP Pub/Sub handles ingestion timestamping natively.

    return ESP_OK;
}
