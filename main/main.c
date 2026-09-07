/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 *
 * Edge Telemetry Gateway — Main Entry Point (ESP32-P4 Host)
 *
 * Orchestration only: initializes all subsystems and creates
 * pinned FreeRTOS tasks on the appropriate cores.
 *
 * Core 0: Network stack (Ethernet, Cloud/TLS, MQTT)
 * Core 1: Sensor ingestion (IPC UART, Protobuf decode, buffer)
 */

#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "cloud_transport.h"
#include "companion_ota.h"
#include "diagnostics.h"
#include "eth_manager.h"
#include "ipc_transport.h"
#include "offline_spooler.h"
#include "storage_manager.h"
#include "telemetry_buffer.h"
#include "telemetry_decoder.h"

static const char *TAG = "gateway_main";

#ifdef CONFIG_ENABLE_MOCK_TELEMETRY
#include "esp_mac.h"
#include "esp_random.h"
#include "telemetry.pb.h"

static void generate_uuid_v4(char *out)
{
    uint8_t rnd[16];
    esp_fill_random(rnd, sizeof(rnd));
    rnd[6] = (rnd[6] & 0x0f) | 0x40;  // Version 4
    rnd[8] = (rnd[8] & 0x3f) | 0x80;  // Variant 1
    sprintf(out, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", rnd[0],
            rnd[1], rnd[2], rnd[3], rnd[4], rnd[5], rnd[6], rnd[7], rnd[8], rnd[9], rnd[10],
            rnd[11], rnd[12], rnd[13], rnd[14], rnd[15]);
}

static void mock_telemetry_task(void *arg)
{
    ESP_LOGW(TAG, "MOCK TELEMETRY TASK STARTED! (Testing mode only)");

    char gateway_id[18] = {0};
    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_ETH) == ESP_OK) {
        sprintf(gateway_id, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4],
                mac[5]);
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));  // Generate fake data every 5 seconds

        telemetry_TelemetryPayload payload = telemetry_TelemetryPayload_init_zero;
        payload.protocol_version = 1;
        payload.schema_version = 21;

        generate_uuid_v4(payload.event_id);
        strncpy(payload.gateway_id, gateway_id, sizeof(payload.gateway_id));
        strncpy(payload.device_id, "MOCK:00:11:22:33", sizeof(payload.device_id));
        payload.node_sequence = 9999;

        payload.measured_at_ms = 1700000000000;
        payload.ingested_at_ms = 1700000000100;

        payload.has_temperature = true;
        payload.temperature = 24.5f;
        payload.has_humidity = true;
        payload.humidity = 45.2f;
        payload.has_pressure = true;
        payload.pressure = 1013.2f;
        payload.has_co2 = true;
        payload.co2 = 450;

        if (telemetry_buffer_push(&payload) == ESP_OK) {
            ESP_LOGI(TAG, "Injected mock payload into RAM queue");
        } else {
            ESP_LOGE(TAG, "Failed to inject mock payload (Queue full?)");
        }
    }
}
#endif

void app_main(void)
{
    ESP_LOGI(TAG, "=== Edge Telemetry Gateway v0.1.0 ===");
    ESP_LOGI(TAG, "Target: ESP32-P4-WIFI6-POE-ETH (Waveshare)");

    /* ── Phase 0: NVS ─────────────────────────────────────── */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* ── Phase 1: Storage & VFS (MicroSD) ─────────────────── */
    if (storage_manager_init() != ESP_OK) {
        ESP_LOGW(TAG, "Storage Manager running in DEGRADED MODE (RAM only).");
    } else {
        offline_spooler_init();
    }

    /* ── Phase 2: Telemetry Buffer & Decoder ──────────────── */
    ESP_ERROR_CHECK(telemetry_buffer_init());
    ESP_ERROR_CHECK(telemetry_decoder_init());
    ESP_LOGI(TAG, "Telemetry buffer initialized (depth=%d)", CONFIG_TELEMETRY_QUEUE_SIZE);

    /* ── Phase 2: Ethernet (Core 0) ───────────────────────── */
    ESP_ERROR_CHECK(eth_manager_init());
    ESP_LOGI(TAG, "Ethernet manager initialized (MDC=%d, MDIO=%d)", CONFIG_ETH_MDC_GPIO,
             CONFIG_ETH_MDIO_GPIO);

    /* ── Phase 3: IPC Transport — UART from C6 (Core 1) ──── */
    ESP_ERROR_CHECK(ipc_transport_init());
    ESP_LOGI(TAG, "IPC transport initialized (TX=%d, RX=%d, baud=%d)", CONFIG_IPC_UART_TX_GPIO,
             CONFIG_IPC_UART_RX_GPIO, CONFIG_IPC_UART_BAUD_RATE);

    /* ── Phase 4: Cloud Transport (Core 0) ────────────────── */
    ESP_ERROR_CHECK(cloud_transport_init());
    ESP_LOGI(TAG, "Cloud transport initialized (project=%s, topic=%s)", CONFIG_GCP_PROJECT_ID,
             CONFIG_GCP_PUB_SUB_TOPIC);

    /* ── Phase 5: Diagnostics (Core 1) ────────────────────── */
    ESP_ERROR_CHECK(diagnostics_init());

#ifdef CONFIG_ENABLE_MOCK_TELEMETRY
    /* ── Phase 6: Mock Injector (Core 1) ──────────────────── */
    // xTaskCreatePinnedToCore(mock_telemetry_task, "mock_telemetry", 4096, NULL, 4, NULL, 1);
    ESP_LOGI(TAG, "Mock Telemetry explicitly DISABLED for E2E Phase 2 test.");
#endif

    ESP_LOGI(TAG, "All subsystems initialized. Gateway is operational.");
}
