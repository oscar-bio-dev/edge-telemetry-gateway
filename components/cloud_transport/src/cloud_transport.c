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
        bool from_spooler = false;
        uint32_t current_cursor = 0;
        uint32_t next_cursor = 0;

        telemetry_TelemetryPayload payloads[BATCH_SIZE];
        size_t count = telemetry_buffer_pop_batch(payloads, BATCH_SIZE);

        if (count == 0 && s_is_online) {
            // Check Spooler
            current_cursor = offline_spooler_get_cursor();
            uint32_t temp_cursor = current_cursor;
            for (size_t i = 0; i < BATCH_SIZE; i++) {
                uint8_t pb_buffer[256];
                uint16_t pb_len = 0;
                if (offline_spooler_peek(temp_cursor, pb_buffer, sizeof(pb_buffer), &pb_len,
                                         &next_cursor) == ESP_OK) {
                    pb_istream_t stream = pb_istream_from_buffer(pb_buffer, pb_len);
                    if (pb_decode(&stream, telemetry_TelemetryPayload_fields, &payloads[count])) {
                        count++;
                        temp_cursor = next_cursor;
                    } else {
                        ESP_LOGW(TAG, "Corrupt payload in spooler, skipping item");
                        temp_cursor = next_cursor;  // skip corrupt
                    }
                } else {
                    break;
                }
            }
            if (count > 0) {
                from_spooler = true;
                next_cursor = temp_cursor;
            }
        }

        if (count == 0) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        refresh_jwt_if_needed();

        char json_payload[2048] = "{\"messages\":[";
        size_t json_len = strlen(json_payload);

        for (size_t i = 0; i < count; i++) {
            uint8_t pb_buffer[256];
            pb_ostream_t stream = pb_ostream_from_buffer(pb_buffer, sizeof(pb_buffer));

            if (strlen(payloads[i].gateway_id) == 0) {
                strncpy(payloads[i].gateway_id, gateway_id, sizeof(payloads[i].gateway_id));
            }
            // Stamp ingestion time
            if (payloads[i].ingested_at_ms == 0) {
                payloads[i].ingested_at_ms = (uint64_t)time(NULL) * 1000ULL;
            }

            if (!pb_encode(&stream, telemetry_TelemetryPayload_fields, &payloads[i])) {
                ESP_LOGE(TAG, "Protobuf encoding failed: %s", PB_GET_ERROR(&stream));
                continue;
            }

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
                if (from_spooler) {
                    offline_spooler_commit(next_cursor);
                    offline_spooler_compact();
                }
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
            if (!from_spooler) {
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

        esp_http_client_config_t config = {
            .url = CONFIG_GCP_PUBSUB_PULL_ENDPOINT,
            .method = HTTP_METHOD_POST,
            .timeout_ms = 5000,
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_http_client_set_header(client, "Content-Type", "application/json");

        char auth_header[768];
        snprintf(auth_header, sizeof(auth_header), "Bearer %s", s_cached_jwt);
        esp_http_client_set_header(client, "Authorization", auth_header);

        const char *pull_payload = "{\"maxMessages\":10}";
        esp_http_client_set_post_field(client, pull_payload, strlen(pull_payload));

        char *response_buf = malloc(4096);
        if (!response_buf) {
            esp_http_client_cleanup(client);
            continue;
        }
        memset(response_buf, 0, 4096);

        esp_err_t err = esp_http_client_open(client, strlen(pull_payload));
        if (err == ESP_OK) {
            esp_http_client_write(client, pull_payload, strlen(pull_payload));
            esp_http_client_fetch_headers(client);
            int status = esp_http_client_get_status_code(client);
            if (status == 200) {
                int read_len = esp_http_client_read(client, response_buf, 4095);
                if (read_len > 0) {
                    response_buf[read_len] = '\0';
                }
                cJSON *root = cJSON_Parse(response_buf);
                if (root) {
                    cJSON *msgs = cJSON_GetObjectItem(root, "receivedMessages");
                    if (cJSON_IsArray(msgs)) {
                        int count = cJSON_GetArraySize(msgs);
                        ESP_LOGI(TAG, "Pulled %d messages from Pub/Sub Downlink", count);

                        cJSON *ack_req = cJSON_CreateObject();
                        cJSON *ack_ids = cJSON_CreateArray();
                        cJSON_AddItemToObject(ack_req, "ackIds", ack_ids);

                        for (int i = 0; i < count; i++) {
                            cJSON *msg_obj = cJSON_GetArrayItem(msgs, i);
                            cJSON *ack_id = cJSON_GetObjectItem(msg_obj, "ackId");
                            cJSON *message = cJSON_GetObjectItem(msg_obj, "message");
                            if (ack_id && message) {
                                cJSON_AddItemToArray(ack_ids,
                                                     cJSON_CreateString(ack_id->valuestring));

                                cJSON *data = cJSON_GetObjectItem(message, "data");
                                if (data && data->valuestring) {
                                    unsigned char decoded[256];
                                    size_t olen = 0;
                                    mbedtls_base64_decode(decoded, sizeof(decoded), &olen,
                                                          (const unsigned char *)data->valuestring,
                                                          strlen(data->valuestring));
                                    decoded[olen] = '\0';

                                    // Try to parse JSON command
                                    cJSON *cmd_json = cJSON_Parse((const char *)decoded);
                                    if (cmd_json) {
                                        cJSON *mac = cJSON_GetObjectItem(cmd_json, "mac");
                                        cJSON *cmd = cJSON_GetObjectItem(cmd_json, "cmd");
                                        if (mac && mac->valuestring && cmd) {
                                            uint8_t target_mac[6];
                                            if (sscanf(mac->valuestring,
                                                       "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                                                       &target_mac[0], &target_mac[1],
                                                       &target_mac[2], &target_mac[3],
                                                       &target_mac[4], &target_mac[5]) == 6) {
                                                uint8_t ipc_payload[7];
                                                memcpy(ipc_payload, target_mac, 6);
                                                ipc_payload[6] = (uint8_t)cmd->valueint;
                                                ipc_transport_send(IPC_HDR_CMD_INJECT, ipc_payload,
                                                                   sizeof(ipc_payload));
                                                ESP_LOGI(TAG, "Enqueued Downlink CMD %d for %s",
                                                         cmd->valueint, mac->valuestring);
                                            }
                                        }
                                        cJSON_Delete(cmd_json);
                                    }
                                }
                            }
                        }

                        // Send Acknowledge
                        if (cJSON_GetArraySize(ack_ids) > 0) {
                            char *ack_payload = cJSON_PrintUnformatted(ack_req);
                            if (ack_payload) {
                                char ack_url[256];
                                strncpy(ack_url, CONFIG_GCP_PUBSUB_PULL_ENDPOINT, sizeof(ack_url));
                                char *pull_suffix = strstr(ack_url, ":pull");
                                if (pull_suffix) {
                                    strcpy(pull_suffix, ":acknowledge");
                                    esp_http_client_config_t ack_config = {
                                        .url = ack_url,
                                        .method = HTTP_METHOD_POST,
                                        .timeout_ms = 5000,
                                    };
                                    esp_http_client_handle_t ack_client =
                                        esp_http_client_init(&ack_config);
                                    esp_http_client_set_header(ack_client, "Content-Type",
                                                               "application/json");
                                    esp_http_client_set_header(ack_client, "Authorization",
                                                               auth_header);
                                    esp_http_client_set_post_field(ack_client, ack_payload,
                                                                   strlen(ack_payload));
                                    if (esp_http_client_perform(ack_client) == ESP_OK) {
                                        ESP_LOGI(TAG, "Acknowledged %d messages",
                                                 cJSON_GetArraySize(ack_ids));
                                    }
                                    esp_http_client_cleanup(ack_client);
                                }
                                free(ack_payload);
                            }
                        }
                        cJSON_Delete(ack_req);
                    }
                    cJSON_Delete(root);
                }
            }
        }
        esp_http_client_cleanup(client);
        free(response_buf);
    }
}

esp_err_t cloud_transport_init(void)
{
    ESP_LOGI(TAG, "Initializing Cloud Transport (GCP Pub/Sub) via mTLS...");

    // Create the publisher task on Core 0 (Networking Core)
    xTaskCreatePinnedToCore(gcp_publisher_task, "gcp_publisher", 16384, NULL, 5, NULL, 0);

    // Create the subscriber task for Downlink Commands
    xTaskCreatePinnedToCore(gcp_subscriber_task, "gcp_subscriber", 8192, NULL, 4, NULL, 0);

    return ESP_OK;
}

bool cloud_transport_is_connected(void)
{
    return s_is_online;
}
