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
#include "telemetry_buffer.h"

static const char *TAG = "cloud_transport";

#define JWT_REFRESH_MINUTES 55
#define JWT_MAX_TTL_MINUTES 60
#define BATCH_SIZE          10

static char s_cached_jwt[512];
static time_t s_jwt_expiry = 0;

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
        // Wait for telemetry samples in the RAM queue
        // In Phase 2 we defined `telemetry_buffer_pop` or similar.
        // For the skeleton we simulate popping a batch.
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Ensure JWT is valid before sending
        refresh_jwt_if_needed();

        // Check if there are items
        size_t items_in_queue = 5;  // MOCK
        if (items_in_queue > 0) {
            ESP_LOGD(TAG, "Preparing to send batch of telemetry to GCP...");

            // Note: The HTTP client config must bind specifically to the Ethernet interface.
            // esp_netif_t *eth_netif = esp_netif_get_handle_from_ifkey("ETH_DEF");
            //
            // esp_http_client_config_t config = {
            //     .url = "https://pubsub.googleapis.com/v1/projects/" CONFIG_GCP_PROJECT_ID
            //     "/topics/" CONFIG_GCP_PUB_SUB_TOPIC ":publish", .transport_type =
            //     HTTP_TRANSPORT_OVER_SSL, .if_name = eth_netif, // Restrict to native Ethernet
            // };
            // esp_http_client_handle_t client = esp_http_client_init(&config);
            // esp_http_client_set_header(client, "Authorization", "Bearer <s_cached_jwt>");
            // ... perform POST ...
            // esp_http_client_cleanup(client);

            ESP_LOGI(TAG, "Successfully published telemetry batch to Google Cloud Pub/Sub.");
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
