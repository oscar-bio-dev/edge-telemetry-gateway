/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cli_manager.h"
#include <stdio.h>
#include <string.h>
#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_log.h"
#include "ipc_transport.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "cli_manager";

// CLI arguments
static struct {
    struct arg_str *mac;
    struct arg_str *lmk;
    struct arg_end *end;
} add_node_args;

// Helper to parse MAC
static bool parse_mac(const char *mac_str, uint8_t *mac_bytes)
{
    if (sscanf(mac_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac_bytes[0], &mac_bytes[1],
               &mac_bytes[2], &mac_bytes[3], &mac_bytes[4], &mac_bytes[5]) == 6) {
        return true;
    }
    return false;
}

static void inject_peer_to_c6(const uint8_t *mac, const uint8_t *lmk)
{
    ipc_transport_send_add_peer(mac, lmk);
}

// NVS helpers
#define PEERS_NAMESPACE "peers"

typedef struct {
    uint8_t mac[6];
    uint8_t lmk[16];
} stored_peer_t;

static esp_err_t store_peer_in_nvs(const uint8_t *mac, const uint8_t *lmk)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(PEERS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
        return err;

    uint32_t count = 0;
    nvs_get_u32(handle, "count", &count);

    char key[16];
    snprintf(key, sizeof(key), "peer_%lu", count);

    stored_peer_t peer;
    memcpy(peer.mac, mac, 6);
    memset(peer.lmk, 0, 16);
    size_t lmk_len = strlen((const char *)lmk);
    if (lmk_len > 16)
        lmk_len = 16;
    memcpy(peer.lmk, lmk, lmk_len);

    err = nvs_set_blob(handle, key, &peer, sizeof(peer));
    if (err == ESP_OK) {
        count++;
        nvs_set_u32(handle, "count", count);
        nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static void load_and_inject_peers(void)
{
    nvs_handle_t handle;
    if (nvs_open(PEERS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        ESP_LOGI(TAG, "No stored peers found.");
        return;
    }

    uint32_t count = 0;
    nvs_get_u32(handle, "count", &count);
    ESP_LOGI(TAG, "Found %lu stored peers. Injecting to C6...", count);

    for (uint32_t i = 0; i < count; i++) {
        char key[16];
        snprintf(key, sizeof(key), "peer_%lu", i);
        stored_peer_t peer;
        size_t len = sizeof(peer);
        if (nvs_get_blob(handle, key, &peer, &len) == ESP_OK && len == sizeof(peer)) {
            inject_peer_to_c6(peer.mac, peer.lmk);
        }
    }
    nvs_close(handle);
}

static int cmd_node_add(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&add_node_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, add_node_args.end, argv[0]);
        return 1;
    }

    uint8_t mac[6];
    if (!parse_mac(add_node_args.mac->sval[0], mac)) {
        printf("Error: Invalid MAC format. Use XX:XX:XX:XX:XX:XX\n");
        return 1;
    }

    const char *lmk_str = add_node_args.lmk->sval[0];
    if (strlen(lmk_str) > 16) {
        printf("Error: LMK too long (max 16 chars)\n");
        return 1;
    }

    uint8_t lmk[16] = {0};
    memcpy(lmk, lmk_str, strlen(lmk_str));

    printf("Saving Peer to NVS...\n");
    if (store_peer_in_nvs(mac, lmk) == ESP_OK) {
        printf("Saved successfully. Injecting to C6...\n");
        inject_peer_to_c6(mac, lmk);
        printf("Node injected!\n");
    } else {
        printf("Failed to save to NVS.\n");
    }

    return 0;
}

esp_err_t cli_manager_init(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();

    // Disable console output prompt temporarily to avoid spam
    repl_config.prompt = "edge-gw> ";

    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));

    add_node_args.mac = arg_str1(NULL, NULL, "<mac>", "MAC Address (XX:XX:XX:XX:XX:XX)");
    add_node_args.lmk = arg_str1(NULL, NULL, "<lmk>", "16-byte LMK string");
    add_node_args.end = arg_end(2);

    const esp_console_cmd_t add_cmd = {
        .command = "node_add",
        .help = "Registra un nuevo Nodo Sensor de confianza y lo inyecta al Modem C6",
        .hint = NULL,
        .func = &cmd_node_add,
        .argtable = &add_node_args};
    ESP_ERROR_CHECK(esp_console_cmd_register(&add_cmd));

    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    // Load existing peers from NVS and send them to C6
    load_and_inject_peers();

    return ESP_OK;
}
