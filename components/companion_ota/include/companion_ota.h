/*
 * SPDX-FileCopyrightText: 2026 EcoTech Sensing
 * SPDX-License-Identifier: Apache-2.0
 *
 * Companion OTA — Host-Driven firmware update for ESP32-C6 via esp-serial-flasher.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t companion_ota_flash(const uint8_t *firmware, size_t firmware_size);

#ifdef __cplusplus
}
#endif
