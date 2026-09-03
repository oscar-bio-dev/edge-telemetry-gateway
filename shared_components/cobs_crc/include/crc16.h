/*
 * SPDX-FileCopyrightText: 2026 EcoTech Sensing
 * SPDX-License-Identifier: Apache-2.0
 *
 * CRC16-CCITT (polynomial 0x1021, init 0xFFFF).
 *
 * Lookup-table implementation for speed on RISC-V cores.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Compute CRC16-CCITT over a data buffer.
 *
 * Uses polynomial 0x1021 with initial value 0xFFFF (ITU-T standard).
 * This is the same CRC used by XMODEM, Bluetooth, and many embedded protocols.
 *
 * @param[in] data  Pointer to data buffer.
 * @param[in] len   Length of data in bytes.
 *
 * @return 16-bit CRC value.
 */
uint16_t crc16_ccitt(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif
