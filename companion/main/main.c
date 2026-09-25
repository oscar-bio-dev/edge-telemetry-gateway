/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 *
 * Edge Companion C6 — Main Entry Point
 *
 * Dedicated ESP-NOW receiver with autonomous Edge-ACK capability.
 * Receives broadcast telemetry from sensor nodes, sends immediate ACKs
 * (with epoch and optional piggybacked commands from the Mailbox), then
 * forwards the raw payload to ESP32-P4 Host via UART/COBS.
 *
 * This firmware is intentionally minimal: no TCP/IP, no TLS, no cloud.
 * The C6 acts as a pure radio-to-serial bridge ("Smart Proxy") with
 * autonomous ACK capability for sub-20ms response times.
 */

#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "espnow_receiver.h"
#include "heartbeat.h"
#include "ipc_sender.h"
#include "mailbox.h"

static const char *TAG = "companion_main";

void app_main(void)
{
    ESP_LOGI(TAG, "=== Edge Companion C6 v0.2.0 ===");
    ESP_LOGI(TAG, "Mode: ESP-NOW Smart Proxy + Edge-ACK (Channel %d)", CONFIG_ESPNOW_CHANNEL);

    /* ── NVS ───────────────────────────────────────────────── */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* ── Mailbox (must be ready before IPC and ESP-NOW) ───── */
    ESP_ERROR_CHECK(mailbox_init());
    ESP_LOGI(TAG, "Mailbox initialized (%d slots)", GW_MAILBOX_MAX_ENTRIES);

    /* ── IPC Sender (UART TX/RX to P4) — must be ready before receiver ── */
    ESP_ERROR_CHECK(ipc_sender_init());
    ESP_LOGI(TAG, "IPC sender initialized (TX=%d, RX=%d, baud=%d)", CONFIG_IPC_UART_TX_GPIO,
             CONFIG_IPC_UART_RX_GPIO, CONFIG_IPC_UART_BAUD_RATE);

    /* ── ESP-NOW Receiver (creates ack_dispatch + forward tasks) ── */
    ESP_ERROR_CHECK(espnow_receiver_init());
    ESP_LOGI(TAG, "ESP-NOW receiver initialized (channel=%d)", CONFIG_ESPNOW_CHANNEL);

    /* ── Heartbeat ────────────────────────────────────────── */
    ESP_ERROR_CHECK(heartbeat_init());
    ESP_LOGI(TAG, "Heartbeat timer started (%d ms interval)", CONFIG_HEARTBEAT_INTERVAL_MS);

    ESP_LOGI(TAG, "Companion is operational. ACK-First, Forward-Later active.");
}
