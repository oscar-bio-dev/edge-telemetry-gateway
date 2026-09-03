/*
 * SPDX-FileCopyrightText: 2026 EcoTech Sensing
 * SPDX-License-Identifier: Apache-2.0
 *
 * IPC Frame Protocol — Shared between Host (P4) and Companion (C6)
 *
 * Defines the canonical frame structure for inter-chip communication
 * over UART with COBS encoding and CRC16-CCITT integrity checking.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief IPC message types exchanged between P4 Host and C6 Companion.
 */
typedef enum {
    IPC_MSG_TELEMETRY    = 0x01,  /**< Payload = EnvironmentalData (Protobuf) */
    IPC_MSG_HEARTBEAT    = 0x02,  /**< C6 alive signal, no payload */
    IPC_MSG_NODE_JOIN    = 0x03,  /**< New ESP-NOW node detected */
    IPC_MSG_NODE_LEAVE   = 0x04,  /**< Node not seen in N cycles */
    IPC_MSG_C6_STATUS    = 0x10,  /**< C6 firmware version, uptime, etc. */
    IPC_MSG_P4_CMD       = 0x80,  /**< Command from P4 to C6 (channel change, etc.) */
    IPC_MSG_OTA_START    = 0xFE,  /**< P4 signals OTA start to C6 */
    IPC_MSG_ACK          = 0xFF,  /**< Generic acknowledgment */
} ipc_msg_type_t;

/**
 * @brief IPC frame header (pre-COBS encoding).
 *
 * Packed struct transmitted over UART inside COBS-encoded frames.
 * The payload (Protobuf raw bytes) follows immediately after this header.
 *
 * Wire format (before COBS):
 *   [type:1][src_mac:6][seq_num:2][rssi:1][payload:N][crc16:2]
 *
 * Wire format (after COBS):
 *   [0x00][COBS-encoded data][0x00]
 */
typedef struct __attribute__((packed)) {
    uint8_t  type;           /**< Message type (ipc_msg_type_t) */
    uint8_t  src_mac[6];     /**< Source MAC of the ESP-NOW sender node */
    uint16_t seq_num;        /**< Sequence number (detect duplicates/losses) */
    int8_t   rssi;           /**< RSSI of the received ESP-NOW packet */
} ipc_header_t;

/** Minimum frame size: header (10B) + CRC16 (2B), no payload */
#define IPC_FRAME_MIN_SIZE      (sizeof(ipc_header_t) + 2)

/** Maximum payload size (ESP-NOW v2 max = 1470B, but we limit to 250B) */
#define IPC_PAYLOAD_MAX_SIZE    250

/** Maximum raw frame size before COBS encoding */
#define IPC_RAW_FRAME_MAX_SIZE  (sizeof(ipc_header_t) + IPC_PAYLOAD_MAX_SIZE + 2)

/** COBS encoding adds at most 1 byte per 254 input bytes + 1 */
#define IPC_COBS_MAX_ENCODED(n) ((n) + ((n) / 254) + 1)

/** Maximum COBS-encoded frame size (including sentinels) */
#define IPC_ENCODED_FRAME_MAX_SIZE \
    (2 + IPC_COBS_MAX_ENCODED(IPC_RAW_FRAME_MAX_SIZE))

#ifdef __cplusplus
}
#endif
