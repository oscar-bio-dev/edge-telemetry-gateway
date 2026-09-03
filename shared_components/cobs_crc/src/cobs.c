/*
 * SPDX-FileCopyrightText: 2026 EcoTech Sensing
 * SPDX-License-Identifier: Apache-2.0
 *
 * COBS (Consistent Overhead Byte Stuffing) — Implementation
 */

#include "cobs.h"

size_t cobs_encode(const uint8_t *src, size_t src_len, uint8_t *dst)
{
    if (src == NULL || dst == NULL) {
        return 0;
    }

    const uint8_t *src_end = src + src_len;
    uint8_t *code_ptr = dst;  /* Pointer to the code byte slot */
    uint8_t *dst_ptr = dst + 1;
    uint8_t code = 0x01;

    while (src < src_end) {
        if (*src == 0x00) {
            /* Write the run length to the code byte position */
            *code_ptr = code;
            code_ptr = dst_ptr++;
            code = 0x01;
        } else {
            *dst_ptr++ = *src;
            code++;
            if (code == 0xFF) {
                /* Maximum run length reached — emit and restart */
                *code_ptr = code;
                code_ptr = dst_ptr++;
                code = 0x01;
            }
        }
        src++;
    }

    /* Write the final code byte */
    *code_ptr = code;

    return (size_t)(dst_ptr - dst);
}

size_t cobs_decode(const uint8_t *src, size_t src_len, uint8_t *dst)
{
    if (src == NULL || dst == NULL || src_len == 0) {
        return 0;
    }

    const uint8_t *src_end = src + src_len;
    uint8_t *dst_ptr = dst;

    while (src < src_end) {
        uint8_t code = *src++;

        if (code == 0x00) {
            /* Unexpected zero in COBS stream — decode error */
            return 0;
        }

        for (uint8_t i = 1; i < code; i++) {
            if (src >= src_end) {
                /* Truncated frame — decode error */
                return 0;
            }
            *dst_ptr++ = *src++;
        }

        /* If code < 0xFF, a zero byte was removed here — restore it
         * (unless we're at the end of the source data) */
        if (code < 0xFF && src < src_end) {
            *dst_ptr++ = 0x00;
        }
    }

    return (size_t)(dst_ptr - dst);
}
