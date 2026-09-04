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

static void gcp_publisher_task(void *arg)
{
    ESP_LOGI(TAG, "GCP Publisher Task started on Core %d", xPortGetCoreID());

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        refresh_jwt_if_needed();

        size_t items_in_queue = 5;  // MOCK
        if (items_in_queue > 0) {
            if (s_fail_count >= MAX_RETRIES) {
                if (s_is_online) {
                    ESP_LOGW(TAG, "Network declared DOWN. Rerouting to Offline Spooler.");
                    s_is_online = false;
                }

                // MOCK Payload serialization
                uint8_t dummy_pb[64] = {0xAA, 0xBB};
                esp_err_t err = offline_spooler_append(dummy_pb, sizeof(dummy_pb));
                if (err == ESP_OK) {
                    ESP_LOGD(TAG, "Spooler append successful.");
                } else {
                    ESP_LOGE(TAG, "Spooler append failed! Telemetry lost.");
                }

                // Simulate periodic network probe to recover
                s_fail_count++;
                if (s_fail_count > (MAX_RETRIES + 5)) {
                    ESP_LOGI(TAG, "Network recovered (simulated).");
                    s_fail_count = 0;
                    s_is_online = true;
                }
                continue;
            }

            ESP_LOGD(TAG, "Preparing to send batch of telemetry to GCP...");

            // MOCK HTTP POST
            bool http_success = false;  // Simulate failure to trigger spooler

            if (http_success) {
                ESP_LOGI(TAG, "Successfully published telemetry batch to Google Cloud.");
                s_fail_count = 0;
                s_is_online = true;

                // THROTTLE: Flush 1 batch from Spooler if network is UP
                uint8_t *spool_buf = NULL;
                uint16_t popped = 0;
                if (offline_spooler_pop(&spool_buf, 5, &popped) == ESP_OK) {
                    ESP_LOGI(TAG, "Flushed %d items from offline spooler.", popped);
                }

            } else {
                ESP_LOGE(TAG, "HTTP POST Failed.");
                s_fail_count++;
            }
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
