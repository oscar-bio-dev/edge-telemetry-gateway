/*
 * SPDX-FileCopyrightText: 2026 EcoTech Sensing
 * SPDX-License-Identifier: Apache-2.0
 *
 * Heartbeat — Periodic alive signal to P4 Host.
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t heartbeat_init(void);

#ifdef __cplusplus
}
#endif
