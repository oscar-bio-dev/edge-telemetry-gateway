/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#include "espnow_receiver.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "ipc_sender.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#define ESPNOW_WIFI_CHANNEL CONFIG_ESPNOW_CHANNEL

static const char *TAG = "espnow_rx";

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    if (recv_info->src_addr == NULL || data == NULL || len == 0) {
        ESP_LOGW(TAG, "Receive callback with invalid parameters");
        return;
    }

    // In ESP-IDF v5+, rssi is available in rx_ctrl inside recv_info
    int8_t rssi = 0;
    if (recv_info->rx_ctrl) {
        rssi = recv_info->rx_ctrl->rssi;
    }

    // Forward the ESP-NOW payload directly to the P4 Host via UART/COBS
    // We treat the payload as raw Protobuf (IPC_MSG_TELEMETRY).
    esp_err_t err = ipc_sender_send_frame(IPC_MSG_TELEMETRY, recv_info->src_addr, rssi, data, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send frame to Host: %s", esp_err_to_name(err));
    }
}

static esp_err_t wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Set channel to match sensor nodes
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK(esp_wifi_start());
    return ESP_OK;
}

esp_err_t espnow_receiver_init(void)
{
    // Ensure NVS is initialized (required by WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(wifi_init());

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        ESP_LOGE(TAG, "Error initializing ESP-NOW");
        return ESP_FAIL;
    }

    // Register receive callback
    esp_now_register_recv_cb(espnow_recv_cb);

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "ESP-NOW Receiver Initialized. MAC: %02X:%02X:%02X:%02X:%02X:%02X, Channel: %d",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], ESPNOW_WIFI_CHANNEL);

    return ESP_OK;
}
