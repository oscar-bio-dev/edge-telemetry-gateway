/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#include "telemetry_buffer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "sdkconfig.h"

#define TELEMETRY_QUEUE_SIZE CONFIG_TELEMETRY_QUEUE_SIZE

static const char *TAG = "telemetry_buffer";

static QueueHandle_t telemetry_queue = NULL;
static StaticQueue_t telemetry_queue_struct;
static uint8_t telemetry_queue_storage[TELEMETRY_QUEUE_SIZE * sizeof(telemetry_TelemetryPayload)];

esp_err_t telemetry_buffer_init(void)
{
    telemetry_queue = xQueueCreateStatic(TELEMETRY_QUEUE_SIZE, sizeof(telemetry_TelemetryPayload),
                                         telemetry_queue_storage, &telemetry_queue_struct);
    if (telemetry_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create static FreeRTOS queue");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Telemetry RAM Buffer initialized. Capacity: %d items", TELEMETRY_QUEUE_SIZE);
    return ESP_OK;
}

esp_err_t telemetry_buffer_push(const telemetry_TelemetryPayload *payload)
{
    if (telemetry_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    // Push to the back of the queue. Do not block if full.
    if (xQueueSendToBack(telemetry_queue, payload, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Buffer full! Dropping telemetry sample.");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

size_t telemetry_buffer_pop_batch(telemetry_TelemetryPayload *out_array, size_t max_items)
{
    if (telemetry_queue == NULL || out_array == NULL) {
        return 0;
    }

    size_t count = 0;
    while (count < max_items) {
        if (xQueueReceive(telemetry_queue, &out_array[count], 0) == pdTRUE) {
            count++;
        } else {
            break;  // Queue is empty
        }
    }

    return count;
}
