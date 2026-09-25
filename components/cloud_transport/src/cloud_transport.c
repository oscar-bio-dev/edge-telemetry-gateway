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
#include "gateway_headers.h"
#include "ipc_transport.h"
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

#include "esp_mac.h"
#include "esp_random.h"
#include "mbedtls/base64.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "telemetry.pb.h"

// Endpoint is now configured via Kconfig: CONFIG_GCP_PUBSUB_ENDPOINT

static void gcp_publisher_task(void *arg)
{
    ESP_LOGI(TAG, "GCP Publisher Task started on Core %d", xPortGetCoreID());

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_ETH);
    char gateway_id[18];
    sprintf(gateway_id, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4],
            mac[5]);

    while (1) {
        telemetry_TelemetryPayload payloads[BATCH_SIZE];
        size_t count = telemetry_buffer_pop_batch(payloads, BATCH_SIZE);

        if (count == 0) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        refresh_jwt_if_needed();

        // Formato JSON para Pub/Sub: {"messages": [{"data": "base64..."}, ...]}
        char json_payload[2048] = "{\"messages\":[";
        size_t json_len = strlen(json_payload);

        for (size_t i = 0; i < count; i++) {
            uint8_t pb_buffer[256];
            pb_ostream_t stream = pb_ostream_from_buffer(pb_buffer, sizeof(pb_buffer));

            // Inyectar gateway_id en cada mensaje si no está presente
            if (strlen(payloads[i].gateway_id) == 0) {
                strncpy(payloads[i].gateway_id, gateway_id, sizeof(payloads[i].gateway_id));
            }

            if (!pb_encode(&stream, telemetry_TelemetryPayload_fields, &payloads[i])) {
                ESP_LOGE(TAG, "Protobuf encoding failed: %s", PB_GET_ERROR(&stream));
                continue;
            }

            // Base64 encode
            unsigned char base64_buf[512];
            size_t olen = 0;
            mbedtls_base64_encode(base64_buf, sizeof(base64_buf), &olen, pb_buffer,
                                  stream.bytes_written);
            base64_buf[olen] = '\0';

            char msg_obj[600];
            snprintf(msg_obj, sizeof(msg_obj), "{\"data\":\"%s\"}%s", base64_buf,
                     (i < count - 1) ? "," : "");

            if (json_len + strlen(msg_obj) < sizeof(json_payload) - 2) {
                strcat(json_payload, msg_obj);
                json_len += strlen(msg_obj);
            }
        }
        strcat(json_payload, "]}");

        // HTTP POST to Endpoint
        esp_http_client_config_t config = {
            .url = CONFIG_GCP_PUBSUB_ENDPOINT,
            .method = HTTP_METHOD_POST,
            .timeout_ms = 5000,
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);

        esp_http_client_set_header(client, "Content-Type", "application/json");

        char auth_header[768];
        snprintf(auth_header, sizeof(auth_header), "Bearer %s", s_cached_jwt);
        esp_http_client_set_header(client, "Authorization", auth_header);

        esp_http_client_set_post_field(client, json_payload, strlen(json_payload));

        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            int status = esp_http_client_get_status_code(client);
            if (status == 200) {
                ESP_LOGI(TAG, "Successfully published %d payloads to Pub/Sub! (Status 200)", count);
                s_fail_count = 0;
                s_is_online = true;
            } else {
                ESP_LOGE(TAG, "Pub/Sub rejected payload. Status: %d", status);
                s_fail_count++;
            }
        } else {
            ESP_LOGE(TAG, "HTTP POST Failed to reach Pub/Sub: %s", esp_err_to_name(err));
            s_fail_count++;
        }

        esp_http_client_cleanup(client);

        if (s_fail_count >= MAX_RETRIES) {
            if (s_is_online) {
                ESP_LOGW(TAG, "Network declared DOWN. Rerouting to Offline Spooler.");
                s_is_online = false;
            }
            /* Spool all payloads in this failed batch */
            for (size_t i = 0; i < count; i++) {
                uint8_t pb_buffer[256];
                pb_ostream_t stream = pb_ostream_from_buffer(pb_buffer, sizeof(pb_buffer));
                if (pb_encode(&stream, telemetry_TelemetryPayload_fields, &payloads[i])) {
                    offline_spooler_append(pb_buffer, stream.bytes_written);
                }
            }
        }
    }
}

// ============================================================================
// Downlink Spooling (Pull from GCP Pub/Sub)
// ============================================================================
static void gcp_subscriber_task(void *arg)
{
    ESP_LOGI(TAG, "GCP Subscriber Task started on Core %d", xPortGetCoreID());

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(15000));  // Pull every 15 seconds

        if (!s_is_online)
            continue;
        refresh_jwt_if_needed();

        // TODO(Backend): Replace this mock with a real HTTP GET to the GCP Pub/Sub Pull
        // Subscription. We are keeping this as a mock for now until oscar-bio-dev creates the
        // command topic.

        // Mocked injection for Phase 2: Send CMD_RUN_SELF_TEST
        // The IPC_HDR_CMD_INJECT format for C6 Mailbox is:
        // [MAC: 6 bytes] + [Command Enum: 1 byte]
        /*
        uint8_t target_mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
        uint8_t ipc_payload[7];
        memcpy(ipc_payload, target_mac, 6);
        ipc_payload[6] = (uint8_t)telemetry_Command_CMD_RUN_SELF_TEST; // Raw enum, no protobuf!

        ipc_transport_send(IPC_HDR_CMD_INJECT, ipc_payload, sizeof(ipc_payload));
        ESP_LOGI(TAG, "Downlink Spooled: Enqueued CMD_RUN_SELF_TEST to C6 for %02X:%02X...",
                 target_mac[0], target_mac[1]);
        */
    }
}

// ============================================================================
// Offline Spooler Rehydration
// ============================================================================
static void offline_rehydration_task(void *arg)
{
    ESP_LOGI(TAG, "Offline Rehydration Task started");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));

        if (!s_is_online) {
            continue;
        }

        uint8_t pb_buffer[256];
        uint16_t pb_len = 0;

        // Try to pop one item from the spooler
        if (offline_spooler_pop(pb_buffer, sizeof(pb_buffer), &pb_len) == ESP_OK) {
            telemetry_TelemetryPayload payload = telemetry_TelemetryPayload_init_default;
            pb_istream_t stream = pb_istream_from_buffer(pb_buffer, pb_len);

            if (pb_decode(&stream, telemetry_TelemetryPayload_fields, &payload)) {
                ESP_LOGI(TAG, "Rehydrating 1 spooled payload for %s", payload.device_id);
                // Push to ring buffer (will block or drop based on implementation,
                // but since we are online, the publisher will drain it quickly).
                while (telemetry_buffer_push(&payload) != ESP_OK) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                    if (!s_is_online)
                        break;
                }
            }
        }
    }
}

esp_err_t cloud_transport_init(void)
{
    ESP_LOGI(TAG, "Initializing Cloud Transport (GCP Pub/Sub) via mTLS...");

    // Create the publisher task on Core 0 (Networking Core)
    xTaskCreatePinnedToCore(gcp_publisher_task, "gcp_publisher", 16384, NULL, 5, NULL, 0);

    // Create the subscriber task for Downlink Commands
    xTaskCreatePinnedToCore(gcp_subscriber_task, "gcp_subscriber", 8192, NULL, 4, NULL, 0);

    // Create the rehydration task
    xTaskCreatePinnedToCore(offline_rehydration_task, "gcp_rehydrate", 4096, NULL, 3, NULL, 0);

    return ESP_OK;
}

bool cloud_transport_is_connected(void)
{
    return s_is_online;
}
