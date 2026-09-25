# ADR 007: Resiliency and Idempotency in Edge Telemetry Gateway

## Date
2026-09-25

## Status
Accepted

## Context

In an ultra-low-power environmental monitoring network, network instability (Wi-Fi disconnections, ISP outages) and hardware limits (radio collisions) are common. The Edge Telemetry Gateway sits at the chokepoint between thousands of node telemetry packets and the Cloud. 

During our technical audit of the Phase 3 MVP, we found critical vulnerabilities in our Store-and-Forward and Idempotency architecture:
1. **Destructive Reads:** Commands routed from Cloud to C6 were destroyed upon read from the Mailbox, even if the ESP-NOW transmission to the node failed immediately afterward.
2. **Missing Rehydration:** The P4 Offline Spooler stored failed payloads to the MicroSD card, but lacked a mechanism to feed those payloads back into the cloud publisher when the network recovered.
3. **Poison Pills / Duplicate Data:** Retries (either by ESP-NOW node re-transmits or P4 SD card replays) were treated as novel telemetry by the Cloud because the P4 generated a purely random UUIDv4 (`esp_fill_random`) upon ingestion.

## Decisions

### 1. Non-Destructive Mailbox (Peek & Confirm)
Instead of a destructive `mailbox_take()` on the C6, we now use a two-step pattern:
- **`mailbox_peek()`**: Reads the pending command for a specific MAC address without removing it.
- **`mailbox_confirm()`**: Deletes the command from the mailbox **only** if `esp_now_send()` returns `ESP_OK`.
This ensures that if the node goes out of range or RF interference occurs, the command remains spooled for the node's next wake cycle.

### 2. NVS Cursor for Offline Spooler
We implemented `offline_spooler_pop()` as an append-only log reader. 
- A persistent read cursor is stored in Non-Volatile Storage (NVS).
- An asynchronous `offline_rehydration_task` periodically attempts to pop from the Spooler and feeds the parsed `telemetry_TelemetryPayload` back into the main Ring Buffer whenever `s_is_online` is true.
- This creates a seamless re-ingestion flow that utilizes the existing JWT/mTLS publishing logic.

### 3. Deterministic UUIDv4 (MD5 Hashing)
We replaced the random UUID generator with a deterministic MD5 hash of the payload's unique source identity.
- Hash Input: `MAC Address` + `node_sequence` + `sleep_cycles`.
- By converting the resulting 128-bit hash into a valid UUIDv4 structure (forcing Version 4 and Variant 1 bits), the Gateway now acts purely deterministically.
- **Impact:** The Backend (GCP Pub/Sub + BigQuery) can safely de-duplicate messages using the `event_id` column, knowing that any SD Card replay or ESP-NOW retry will strictly generate the exact same `event_id`.

## Consequences

- **Positive:** True zero-data-loss Store-and-Forward. Perfect backend idempotency.
- **Negative:** Increased NVS wear on the P4 due to cursor saving, though mitigated by batch writing and large NVS partitions. NVS commits happen per successfully read block.
- **Negative:** Slight CPU overhead for computing MD5 hashes for every incoming telemetry frame on Core 1, though well within the capabilities of the P4's 400MHz processor.
