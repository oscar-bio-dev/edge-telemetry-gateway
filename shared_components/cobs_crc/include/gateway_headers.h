/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 *
 * Gateway Headers — Shared Protocol Constants
 *
 * Defines the canonical byte headers used across the entire ecosystem:
 *   - ESP-NOW Uplink/Downlink (Node ↔ C6)
 *   - IPC UART (C6 ↔ P4)
 *
 * This file is shared between Host (P4) and Companion (C6) firmwares.
 * Both sides MUST use the same header values for interoperability.
 *
 * Wire format:
 *   ESP-NOW: [header:1][protobuf_payload:N]
 *   IPC:     [COBS([ipc_header:10][espnow_header:1][payload:N][crc16:2])][0x00]
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * §1. ESP-NOW Headers (Radio Layer: Node ↔ C6)
 *
 * These bytes are prepended to every ESP-NOW frame before the Protobuf
 * payload. The C6 reads byte[0] to decide whether to ACK, inject a command,
 * or simply forward the payload to P4.
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Uplink: Node → Gateway (Telemetry) */
#define ESPNOW_HDR_TELEMETRY        0x10

/** Uplink: Node → Gateway (Diagnostic Report after Self-Test) */
#define ESPNOW_HDR_DIAGNOSTIC       0x11

/** Downlink: Gateway → Node (Standard ACK with Epoch) */
#define ESPNOW_HDR_ACK              0x20

/** Downlink: Gateway → Node (Tactical Command Injection) */
#define ESPNOW_HDR_CMD              0x21

/* ═══════════════════════════════════════════════════════════════════════════
 * §2. IPC Headers (UART Layer: P4 ↔ C6)
 *
 * These headers travel inside COBS/CRC16 frames over UART.
 * They allow the P4 to push time sync and pending commands to the C6,
 * and the C6 to notify the P4 about command delivery status.
 * ═══════════════════════════════════════════════════════════════════════════ */

/** P4 → C6: Epoch synchronization (payload: uint64_t epoch_s) */
#define IPC_HDR_SYNC_EPOCH          0x30

/** P4 → C6: Enqueue command for a node (payload: MAC[6] + Command Protobuf) */
#define IPC_HDR_CMD_INJECT          0x31

/** C6 → P4: Confirm command was delivered to node (payload: MAC[6]) */
#define IPC_HDR_CMD_DELIVERED       0x32

/* ═══════════════════════════════════════════════════════════════════════════
 * §3. Mailbox Configuration (C6 RAM)
 *
 * The Mailbox is a fixed-size array in C6 RAM that holds pending commands
 * until the target node wakes up and transmits telemetry.
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Maximum number of simultaneous pending commands in the C6 Mailbox */
#define GW_MAILBOX_MAX_ENTRIES      16

/** Maximum payload size for a single command (GatewayAck Protobuf) */
#define GW_MAILBOX_CMD_MAX_SIZE     32

/** ACK response deadline (must be < node's RX window of 50ms) */
#define GW_ACK_DEADLINE_MS          20

#ifdef __cplusplus
}
#endif
