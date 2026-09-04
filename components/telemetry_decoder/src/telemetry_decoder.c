/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#include "telemetry_decoder.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
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

    // Note: The timestamp is left empty (0) as per architectural decision,
    // since we do not have an SNTP synchronized RTC on the P4 yet, and
    // GCP Pub/Sub handles ingestion timestamping natively.

    return ESP_OK;
}
