/*
 * SPDX-FileCopyrightText: 2026 oscar-bio-dev
 * SPDX-License-Identifier: Apache-2.0
 *
 * Mailbox — Pending command store for the C6 Companion.
 *
 * The Mailbox holds commands received from the P4 (via IPC_HDR_CMD_INJECT)
 * until the target node wakes up and transmits telemetry. When the node
 * is seen, the ACK dispatch task looks up its MAC here and piggybacks the
 * command payload onto the GatewayAck.
 *
 * Design:
 *   - Fixed-size static array (no heap allocation)
 *   - O(N) linear scan (N ≤ 16, negligible at 160MHz)
 *   - Thread-safe via FreeRTOS mutex
 *   - Entries expire after a configurable TTL (default 5 min)
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "gateway_headers.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief A single pending command entry in the Mailbox.
 */
typedef struct {
    uint8_t mac[6];                           /**< Target node MAC address */
    uint8_t payload[GW_MAILBOX_CMD_MAX_SIZE]; /**< GatewayAck Protobuf bytes */
    size_t payload_len;                       /**< Length of payload */
    uint64_t expire_epoch;                    /**< Epoch (seconds) after which entry is stale */
    bool occupied;                            /**< True if this slot holds a valid entry */
} mailbox_entry_t;

/**
 * @brief Initialize the Mailbox subsystem.
 *
 * Creates the internal mutex. Must be called once at boot.
 *
 * @return ESP_OK on success.
 */
esp_err_t mailbox_init(void);

/**
 * @brief Insert or overwrite a pending command for a target node.
 *
 * If an entry for the same MAC already exists, it is overwritten.
 * If the mailbox is full and no matching MAC is found, returns ESP_ERR_NO_MEM.
 *
 * @param[in] mac         Target node MAC (6 bytes).
 * @param[in] payload     Serialized GatewayAck Protobuf.
 * @param[in] payload_len Length of payload.
 * @return ESP_OK on success, ESP_ERR_NO_MEM if full.
 */
esp_err_t mailbox_put(const uint8_t *mac, const uint8_t *payload, size_t payload_len);

/**
 * @brief Look up and consume a pending command for a given MAC.
 *
 * If a valid, non-expired entry exists for the given MAC, copies the
 * payload into out_payload, sets out_len, marks the slot as free,
 * and returns true.
 *
 * @param[in]  mac         Source MAC to look up (6 bytes).
 * @param[out] out_payload Buffer to receive the command payload.
 * @param[out] out_len     Receives the length of the payload.
 * @return true if a command was found and consumed, false otherwise.
 */
bool mailbox_take(const uint8_t *mac, uint8_t *out_payload, size_t *out_len);

/**
 * @brief Purge expired entries from the Mailbox.
 *
 * Should be called periodically (e.g., from heartbeat task).
 *
 * @param[in] now_epoch Current epoch in seconds.
 * @return Number of entries purged.
 */
int mailbox_purge_expired(uint64_t now_epoch);

/**
 * @brief Update the cached epoch (called when P4 sends IPC_HDR_SYNC_EPOCH).
 */
void mailbox_set_epoch(uint64_t epoch);

/**
 * @brief Get the cached epoch.
 */
uint64_t mailbox_get_epoch(void);

#ifdef __cplusplus
}
#endif
