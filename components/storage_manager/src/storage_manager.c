/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#include "storage_manager.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include "driver/sdmmc_host.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

static const char *TAG = "storage_mgr";

// Mount path
#define MOUNT_POINT "/sdcard"

// Global state
static bool s_degraded_mode = false;
static esp_ldo_channel_handle_t s_sd_ldo_handle = NULL;
static sdmmc_card_t *s_card = NULL;

static esp_err_t init_ldo_power(void)
{
    ESP_LOGI(TAG, "Initializing PMU LDO channel %d for MicroSD power...",
             CONFIG_SDCARD_LDO_CHANNEL);
    esp_ldo_channel_config_t ldo_cfg = {.chan_id = CONFIG_SDCARD_LDO_CHANNEL,
                                        .voltage_mv = 3300,
                                        .voltage_stable_delay_us = 10000,
                                        .flags = {
                                            .adjustable = 0,
                                        }};

    esp_err_t ret = esp_ldo_acquire_channel(&ldo_cfg, &s_sd_ldo_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to acquire LDO channel %d (err 0x%x)", CONFIG_SDCARD_LDO_CHANNEL,
                 ret);
        return ret;
    }
    ESP_LOGI(TAG, "LDO initialized and stable at 3.3V.");
    return ESP_OK;
}

static void create_vfs_directories(void)
{
    const char *dirs[] = {MOUNT_POINT "/spool", MOUNT_POINT "/ai", MOUNT_POINT "/ai/models",
                          MOUNT_POINT "/ai/history"};

    for (int i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        struct stat st = {0};
        if (stat(dirs[i], &st) == -1) {
            ESP_LOGI(TAG, "Creating directory: %s", dirs[i]);
            mkdir(dirs[i], 0777);
        } else {
            ESP_LOGD(TAG, "Directory already exists: %s", dirs[i]);
        }
    }
}

esp_err_t storage_manager_init(void)
{
    esp_err_t ret;

    // 1. Initialize PMU / LDO for the MicroSD
    ret = init_ldo_power();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "LDO failure. Falling back to DEGRADED_MODE.");
        s_degraded_mode = true;
        return ESP_FAIL;
    }

    // 2. Configure SDMMC Host (Slot 0 for ESP32-P4)
    ESP_LOGI(TAG, "Mounting FATFS over SDMMC Host (Slot 0)...");

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    // Use maximum speed possible, 4-bit mode is default in SDMMC_HOST_DEFAULT()
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    // 3. Configure Slot (Pins 39-44 are default for Slot 0 on P4, managed by the driver)
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;
    // P4 Waveshare requires internal pull-ups on CMD and DATA lines
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    // 4. Configure FATFS mount
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,  // Never format without user intent
        .max_files = 10,
        .allocation_unit_size = 16 * 1024};

    // 5. Mount the filesystem
    ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount FATFS. Card might be formatted wrong.");
        } else if (ret == ESP_ERR_NO_MEM) {
            ESP_LOGE(TAG, "Failed to initialize VFS, not enough memory.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s).", esp_err_to_name(ret));
        }

        ESP_LOGW(TAG, "MicroSD Mount Failed. Entering DEGRADED_MODE.");
        s_degraded_mode = true;

        // We do NOT abort. We return gracefully so main.c continues.
        return ESP_FAIL;
    }

    // Print card info
    sdmmc_card_print_info(stdout, s_card);

    // 6. Bootstrap directories
    create_vfs_directories();

    ESP_LOGI(TAG, "MicroSD VFS Ready. Storage Manager Initialized.");
    s_degraded_mode = false;
    return ESP_OK;
}

bool storage_manager_is_degraded(void)
{
    return s_degraded_mode;
}
