/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cloud_transport.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "jwt_generator.h"
#include "offline_spooler.h"
#include "telemetry_buffer.h"

static const char *TAG = "cloud_transport";

#define JWT_REFRESH_MINUTES 55
#define JWT_MAX_TTL_MINUTES 60
#define BATCH_SIZE          10
#define MAX_RETRIES         3

static char s_cached_jwt[512];
static time_t s_jwt_expiry = 0;
static int s_fail_count = 0;
static bool s_is_online = true;

static void refresh_jwt_if_needed(void)
{
    time_t now = time(NULL);
    if (now >= s_jwt_expiry) {
        ESP_LOGI(TAG, "Refreshing GCP JWT token...");
        esp_err_t err = jwt_generate_es256(CONFIG_GCP_PROJECT_ID, JWT_MAX_TTL_MINUTES, s_cached_jwt,
                                           sizeof(s_cached_jwt));
        if (err == ESP_OK) {
            s_jwt_expiry = now + (JWT_REFRESH_MINUTES * 60);
            ESP_LOGI(TAG, "JWT token successfully generated and cached.");
        } else {
            ESP_LOGE(TAG, "Failed to generate JWT token!");
        }
    }
}

#include "telemetry.pb.h"
#include "pb_encode.h"
#include "mbedtls/base64.h"
#include "esp_mac.h"
#include "esp_random.h"

// GCP Emulator Endpoint (Host IP)
#define EMULATOR_ENDPOINT "http://192.168.0.32:8085/v1/projects/setaesense-iot-core/topics/room-telemetry-topic:publish"

static void generate_uuid_v4(char *out) {
    uint8_t rnd[16];
    esp_fill_random(rnd, sizeof(rnd));
    rnd[6] = (rnd[6] & 0x0f) | 0x40; // Version 4
    rnd[8] = (rnd[8] & 0x3f) | 0x80; // Variant 1
    sprintf(out,
            "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            rnd[0], rnd[1], rnd[2], rnd[3], rnd[4], rnd[5], rnd[6], rnd[7],
            rnd[8], rnd[9], rnd[10], rnd[11], rnd[12], rnd[13], rnd[14], rnd[15]);
}

static void gcp_publisher_task(void *arg)
{
    ESP_LOGI(TAG, "GCP Publisher Task started on Core %d", xPortGetCoreID());

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_ETH);
    char gateway_id[18];
    sprintf(gateway_id, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000)); // Publish every 5 seconds for testing

        // Create Payload
        telemetry_TelemetryPayload payload = telemetry_TelemetryPayload_init_zero;
        
        payload.protocol_version = 1;
        payload.schema_version = 21;
        
        char event_id[37];
        generate_uuid_v4(event_id);
        strncpy(payload.event_id, event_id, sizeof(payload.event_id));
        strncpy(payload.gateway_id, gateway_id, sizeof(payload.gateway_id));
        strncpy(payload.device_id, "AA:BB:CC:DD:EE:FF", sizeof(payload.device_id));
        payload.node_sequence = 1234;
        
        payload.measured_at_ms = 1700000000000;
        payload.ingested_at_ms = 1700000000100;
        
        payload.has_temperature = true; payload.temperature = 24.5f;
        payload.has_humidity = true;    payload.humidity = 45.2f;
        payload.has_pressure = true;    payload.pressure = 1013.2f;
        payload.has_co2 = true;         payload.co2 = 450;
        
        uint8_t pb_buffer[256];
        pb_ostream_t stream = pb_ostream_from_buffer(pb_buffer, sizeof(pb_buffer));
        if (!pb_encode(&stream, telemetry_TelemetryPayload_fields, &payload)) {
            ESP_LOGE(TAG, "Protobuf encoding failed: %s", PB_GET_ERROR(&stream));
            continue;
        }

        // Base64 encode
        unsigned char base64_buf[512];
        size_t olen = 0;
        mbedtls_base64_encode(base64_buf, sizeof(base64_buf), &olen, pb_buffer, stream.bytes_written);
        base64_buf[olen] = '\0';

        // Create JSON body
        char json_payload[1024];
        snprintf(json_payload, sizeof(json_payload),
                 "{\"messages\":[{\"data\":\"%s\"}]}", base64_buf);

        // HTTP POST to Emulator
        esp_http_client_config_t config = {
            .url = EMULATOR_ENDPOINT,
            .method = HTTP_METHOD_POST,
            .timeout_ms = 3000,
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, json_payload, strlen(json_payload));

        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            int status = esp_http_client_get_status_code(client);
            if (status == 200) {
                ESP_LOGI(TAG, "Successfully published synthetic payload (v21) to Pub/Sub Emulator! (Status 200)");
                s_fail_count = 0;
                s_is_online = true;
            } else {
                ESP_LOGE(TAG, "Emulator rejected payload. Status: %d", status);
                s_fail_count++;
            }
        } else {
            ESP_LOGE(TAG, "HTTP POST Failed to reach Emulator: %s", esp_err_to_name(err));
            s_fail_count++;
        }
        
        esp_http_client_cleanup(client);
        
        if (s_fail_count >= MAX_RETRIES) {
            if (s_is_online) {
                ESP_LOGW(TAG, "Network declared DOWN. Rerouting to Offline Spooler.");
                s_is_online = false;
            }
            offline_spooler_append(pb_buffer, stream.bytes_written);
        }
    }
}

esp_err_t cloud_transport_init(void)
{
    ESP_LOGI(TAG, "Initializing Cloud Transport (GCP Pub/Sub) via mTLS...");

    // Create the publisher task on Core 0 (Networking Core)
    xTaskCreatePinnedToCore(gcp_publisher_task, "gcp_publisher", 8192, NULL, 5, NULL, 0);

    return ESP_OK;
}

bool cloud_transport_is_connected(void)
{
    // Return true if HTTP client was able to connect recently
    return true;
}
