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
#include "telemetry_buffer.h"
#include "telemetry_decoder.h"

static const char *TAG = "gateway_main";

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

    /* ── Phase 1: Telemetry Buffer (must be ready before producers) ── */
    ESP_ERROR_CHECK(telemetry_buffer_init());
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

    ESP_LOGI(TAG, "All subsystems initialized. Gateway is operational.");
}
