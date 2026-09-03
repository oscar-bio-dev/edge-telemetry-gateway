/*
 * SPDX-FileCopyrightText: 2026 EcoTech Sensing
 * SPDX-License-Identifier: Apache-2.0
 *
 * Ethernet Manager — EMAC + IP101GRI RMII + lwIP netif.
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t eth_manager_init(void);
bool eth_manager_is_connected(void);

#ifdef __cplusplus
}
#endif
