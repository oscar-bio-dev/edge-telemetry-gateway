/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the SDMMC MicroSD subsystem and mount VFS.
 *
 * This function handles the LDO PMU acquisition for the SD card slot,
 * initializes the SDMMC Host in 4-bit mode (GPIO 39-44 on ESP32-P4),
 * mounts the FAT filesystem at /sdcard, and creates the required VFS
 * directories for Spooling and ESP-DL.
 *
 * If the SD card fails to mount (e.g. missing, broken, unformatted),
 * this function DOES NOT abort. It will log the error and transition
 * the system to DEGRADED_MODE.
 *
 * @return ESP_OK if mounted successfully.
 *         ESP_FAIL or other errors if the mount failed (Degraded Mode).
 */
esp_err_t storage_manager_init(void);

/**
 * @brief Check if the gateway is in Degraded Mode (storage unavailable).
 *
 * @return true if the SD card is offline or failed.
 *         false if the system is operating normally.
 */
bool storage_manager_is_degraded(void);

#ifdef __cplusplus
}
#endif
