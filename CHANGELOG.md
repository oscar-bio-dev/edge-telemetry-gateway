# Changelog

All notable changes to this project will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0-alpha.1] - 2026-09-24

### Added
- **Bidirectional Protocol (Fase 1 — Contratos de Datos):**
  - `telemetry.proto`: Expanded from 21 to 33 fields (aligned with node firmware v0.11.0).
  - New messages: `GatewayAck` (13 bytes, for C6 Edge-ACK) and `DiagnosticReport` (12 bytes).
  - New enums: `NodeStatus` (MONITORING/CALIBRATING/SELF_TESTING/HARDWARE_ERROR) and `Command` (CMD_RUN_SELF_TEST/CMD_REBOOT).
  - `gateway_headers.h`: Canonical header byte definitions for ESP-NOW (0x10-0x21) and IPC (0x30-0x32) protocols.
  - Nanopb `.pb.c`/`.pb.h` regenerated with `nanopb-0.4.9.1`.

## [1.0.0-rc.1] - 2026-09-12

### Added
- Complete End-to-End telemetry flow (ESP-NOW -> ESP32-C6 -> ESP32-P4 -> GCP Pub/Sub).
- Offline Spooler for resilient SD card storage during network outages.
- mTLS authentication for Cloud Transport.

### Changed
- Re-architected C6 flashing procedure via physical BOOT-strap isolation (ADR-006).

### Security
- Excluded development private keys (`.pem`) from version control.
