# Changelog

All notable changes to this project will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0-alpha.2] - 2026-09-25

### Fixed (Technical Audit Block 1 & 2)
- **C6 Companion (Radio layer):**
  - Always send `ESPNOW_HDR_ACK (0x20)` even when piggybacking commands. Fixed dropping of all commands at the node side due to unexpected `0x21` header (C1).
  - Adopted `mailbox_peek()` and `mailbox_confirm()` non-destructive reading. Commands now survive in the mailbox if the ESP-NOW transmission fails (A3).
  - Added periodic mailbox purge `mailbox_purge_expired()` every 64 frames (A4).
  - Removed insecure ESP_LOGW of `CONFIG_ESPNOW_PMK` encryption key (A5).
- **P4 Host (Orchestrator layer):**
  - Fixed `CMD_INJECT` payload format to send 1-byte raw enum instead of Protobuf to C6 Mailbox (C2).
  - Implemented `offline_spooler_pop()` with NVS cursor tracking and added `offline_rehydration_task` to push spooled data back into the ring buffer upon network recovery (A1).
  - Fixed spooler dropping batch items on network failure. Now properly loops and encodes all failed `telemetry_TelemetryPayload` items (A1).
  - Replaced random UUIDv4 generation with deterministic MD5 Hash (`MAC + node_sequence + sleep_cycles`) in `telemetry_decoder.c` for backend idempotency across retries/SD replays (A2).
  - Implemented Nanopb decoding of `telemetry_DiagnosticReport` in `ipc_transport.c` to surface hardware faults (SCD41, BME688, BMV080) in the Gateway logs (M4).

## [1.1.0-alpha.1] - 2026-09-24

### Added
- **Bidirectional Protocol (Fase 1 — Contratos de Datos):**
  - `telemetry.proto`: Expanded from 21 to 33 fields (aligned with node firmware v0.11.0).
  - New messages: `GatewayAck` (13 bytes, for C6 Edge-ACK) and `DiagnosticReport` (12 bytes).
  - New enums: `NodeStatus` (MONITORING/CALIBRATING/SELF_TESTING/HARDWARE_ERROR) and `Command` (CMD_RUN_SELF_TEST/CMD_REBOOT).
  - `gateway_headers.h`: Canonical header byte definitions for ESP-NOW (0x10-0x21) and IPC (0x30-0x32) protocols.
  - Nanopb `.pb.c`/`.pb.h` regenerated with `nanopb-0.4.9.1`.
- **Bidirectional Protocol (Fase 2 — Orquestador P4):**
  - `ipc_transport`: Dynamic routing based on header byte (`0x10` Telemetry, `0x11` Diagnostic).
  - `epoch_sync_task`: Automatic time propagation to C6 via `IPC_HDR_SYNC_EPOCH` (0x30) every 60s.
  - `cloud_transport`: Implemented asynchronous `gcp_subscriber_task` to pull cloud commands and inject them to C6 via `IPC_HDR_CMD_INJECT` (0x31).
- **Bidirectional Protocol (Fase 3 — Companion C6):**
  - Complete rewrite of C6 firmware (`v0.2.0`) to support ACK-First, Forward-Later architecture.
  - `mailbox`: Static RAM pending command store with Epoch-based TTL expiration.
  - Autonomous `GatewayAck` transmission within 6ms, solving the 200ms node wait-window limit.

## [1.0.0-rc.1] - 2026-09-12

### Added
- Complete End-to-End telemetry flow (ESP-NOW -> ESP32-C6 -> ESP32-P4 -> GCP Pub/Sub).
- Offline Spooler for resilient SD card storage during network outages.
- mTLS authentication for Cloud Transport.

### Changed
- Re-architected C6 flashing procedure via physical BOOT-strap isolation (ADR-006).

### Security
- Excluded development private keys (`.pem`) from version control.
