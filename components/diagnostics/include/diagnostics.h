/*
 * SPDX-FileCopyrightText: 2026 EcoTech Sensing
 * SPDX-License-Identifier: Apache-2.0
 *
 * Diagnostics — Health checks, heap stats, companion watchdog.
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t diagnostics_init(void);

#ifdef __cplusplus
}
#endif
