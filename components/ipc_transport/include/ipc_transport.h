/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 *
 * IPC Transport — UART driver for C6 Companion communication.
 *
 * Receives COBS-encoded frames from C6, decodes them, verifies CRC16,
 * and dispatches decoded telemetry into the telemetry buffer.
 * Runs as a FreeRTOS task pinned to Core 1.
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the IPC transport (UART + ingest task on Core 1).
 *
 * Configures UART with parameters from Kconfig and spawns the
 * ipc_ingest_task pinned to Core 1.
 *
 * @return ESP_OK on success, or an error code.
 */
esp_err_t ipc_transport_init(void);

/**
 * @brief Send a command frame to the C6 companion.
 *
 * @param type  Message type (ipc_msg_type_t).
 * @param data  Optional payload data.
 * @param len   Payload length in bytes.
 *
 * @return ESP_OK on success.
 */
esp_err_t ipc_transport_send(uint8_t type, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif
