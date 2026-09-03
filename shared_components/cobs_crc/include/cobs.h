/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 *
 * COBS (Consistent Overhead Byte Stuffing) codec.
 *
 * Zero-allocation implementation suitable for bare-metal / RTOS hot paths.
 * Reference: S. Cheshire & M. Baker, "Consistent Overhead Byte Stuffing",
 *            IEEE/ACM Transactions on Networking, Vol.7 No.2, April 1999.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief COBS-encode a buffer.
 *
 * The encoded output will never contain the byte 0x00, making it
 * suitable for use as a frame delimiter in serial protocols.
 *
 * @param[in]  src      Source data to encode.
 * @param[in]  src_len  Length of source data in bytes.
 * @param[out] dst      Destination buffer for encoded data.
 *                      Must be at least (src_len + src_len/254 + 1) bytes.
 *
 * @return Number of bytes written to dst, or 0 on error.
 */
size_t cobs_encode(const uint8_t *src, size_t src_len, uint8_t *dst);

/**
 * @brief COBS-decode a buffer.
 *
 * Decodes a COBS-encoded block back to its original form.
 * The input must NOT contain 0x00 sentinel bytes (strip them before calling).
 *
 * @param[in]  src      COBS-encoded data (without 0x00 delimiters).
 * @param[in]  src_len  Length of encoded data in bytes.
 * @param[out] dst      Destination buffer for decoded data.
 *                      Must be at least src_len bytes.
 *
 * @return Number of bytes written to dst, or 0 on decode error.
 */
size_t cobs_decode(const uint8_t *src, size_t src_len, uint8_t *dst);

#ifdef __cplusplus
}
#endif
