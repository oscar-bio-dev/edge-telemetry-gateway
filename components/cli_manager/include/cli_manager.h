/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the CLI (esp_console) on the default UART and register commands.
 *
 * Also reads stored peers from NVS and injects them to the C6 via IPC.
 */
esp_err_t cli_manager_init(void);

#ifdef __cplusplus
}
#endif
