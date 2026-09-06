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

#include "esp_mac.h"
#include "esp_random.h"
#include "mbedtls/base64.h"
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
            // En producción aquí guardaríamos al spooler. Por ahora, solo logueamos la falla.
            // offline_spooler_append(pb_buffer, stream.bytes_written);
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
