# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - Unreleased (Archived)

> **NOTICE**: This repository has been officially archived and deprecated.
> All future development has moved to the `edge-s3-gateway` project due to unresolvable hardware conflicts on the ESP32-P4+C6 Waveshare board. The code remains here as an architectural and technical reference for dual-chip IPC over UART.

### Added
- **ADR-006 — Waveshare Post-Mortem**: Comprehensive post-mortem documenting all hardware limitations, workarounds, and failure modes of the Waveshare ESP32-P4-WIFI6-POE-ETH board's dual-chip architecture. Documents the shared CH344Q USB hub as the root cause of irrecoverable C6 flashing failures. Includes board verdict, GPIO control analysis, and migration path to ESP32-P4X-Function-EV-Board. See [`docs/adr/006-waveshare-p4-wifi6-poe-eth-post-mortem.md`](docs/adr/006-waveshare-p4-wifi6-poe-eth-post-mortem.md).
- **CLI Manager Component** (`components/cli_manager/`): Interactive `esp_console` REPL for dynamic ESP-NOW peer provisioning. Supports `node_add <mac> <lmk>` with NVS persistence and IPC forwarding to C6 Companion.
- **C6 Flash Scripts** (`companion/flash_c6.sh`, `companion/flash_c6_slow.sh`): Shell scripts for manual C6 flashing via `/dev/ttyACM5` with `--no-stub` and baud-rate sweep (9600–74880) workarounds.
- **Hardware Flashing Workaround (Waveshare P4 Board)**: Discovered that the P4 and C6 share the same internal UART hub. Implemented a temporary "hack" (tying C6 BOOT to GND via P4 GPIO 54 and freezing the P4) to release the physical UART lines, allowing the C6 to be flashed via an external UART bridge without `esptool` data collision errors.
- **Lab Test 2 Readiness (ESP-NOW E2E)**: Successfully integrated real encrypted ESP-NOW telemetry ingestion. Disabled the `mock_telemetry_task` on the Host (P4) and implemented CCMP encryption with `esp_now_set_pmk` on the Companion (C6). Included simulated peer provisioning (MAC injection via Kconfig) to avoid plaintext handshakes.

### Fixed
- **Companion Wi-Fi Crash Loop (ESP_ERR_WIFI_NOT_STARTED)**: Fixed a fatal initialization bug in the C6 ESP-NOW receiver. Moved `esp_wifi_start()` before `esp_wifi_set_channel()` to strictly comply with the ESP-IDF Wi-Fi driver state machine, successfully stopping the crash loop and allowing the C6 to receive packets.
- **Idempotency Guarantee**: Implemented hardware-accelerated UUIDv4 generation (`esp_fill_random`) in `telemetry_decoder.c` to inject a unique `event_id` into every received payload. This fully aligns the Gateway with the Rust Backend's `ON CONFLICT (event_id, measured_at) DO NOTHING` deduplication logic.
- **Lab Test 1 Success**: Achieved a 100% successful End-to-End data flow! The Gateway (ESP32-P4) generated synthetic Protobuf payloads (Mock Telemetry Task), signed ECDSA JWT tokens on the fly, and published batches to the local GCP Pub/Sub Emulator (`oscar-bio-dev-project/room-telemetry`), which were successfully consumed by the Rust backend.
- **Dynamic Endpoints**: Migrated hardcoded Emulator IP to Kconfig (`CONFIG_GCP_PUBSUB_ENDPOINT`). The defaults are now explicitly aligned with the Rust backend's Pub/Sub emulator topology.

### Changed
- **Project Status → Frozen**: Development on this board has been frozen pending arrival of ESP32-P4X-Function-EV-Board (~Q1 2027). Active development continues in `edge-s3-gateway` (ESP32-S3 + W5500).
- **Architecture Integrity**: Refactored `cloud_transport.c` to strictly pop messages from the RAM buffer (`telemetry_buffer_pop_batch`), removing technical debt where it bypassed the queue.
- **Security Validation**: Re-enabled JWT signature generation and `Authorization` header injection even during local emulator testing, forcing the P4 to validate its crypto cycles (`mbedTLS`) before production.

### Deprecated
- **Host-Driven OTA** (`companion_ota`): Blocked on this board due to unreliable GPIO54/GPIO6 control of C6 EN/BOOT pins. Will be re-evaluated on ESP32-P4X-Function-EV-Board.
- **Waveshare ESP32-P4-WIFI6-POE-ETH Board**: Temporarily abandoned for gateway development (Sep 10, 2026). Will migrate to ESP32-S3 until ESP32-P4X-Function-EV-Board arrives.

### Known Issues
- **C6 USB Flashing Permanently Broken (Soft-Brick)**: The board does not have an internal USB connection for the C6. The external H7 debug header lacks a BOOT pin. The P4 host-driven auto-strapping method fails due to unstable pull-ups/electrical isolation. Consequently, the C6 is permanently locked to its current firmware and cannot be updated. See [ADR-006](docs/adr/006-waveshare-p4-wifi6-poe-eth-post-mortem.md).

### Fixed (Earlier)
- **Stack Protection Fault**: Increased `gcp_publisher_task` stack size from 8192 to 16384 bytes to prevent `Guru Meditation Error` (Stack Overflow) caused by MbedTLS ECDSA cryptographic calculations during JWT generation.
- **True MbedTLS 3 Migration**: Officially aligned `mbedtls_pk_parse_key` and `mbedtls_pk_sign` signatures with ESP-IDF v6.1 PSA Crypto specifications (removed legacy `f_rng` and `ctr_drbg` entropy arguments as PSA handles them internally).
- **Protobuf Mega-Schema v21**: Synchronized `telemetry.proto` and `gateway_health.proto` with the backend's v21 schema. Configured Nanopb static memory allocation for dynamic strings to prevent heap panics.
- **ESP-IDF v6.1 Migration**: Successfully migrated to v6.1. Fixed RISC-V Illegal Instruction panics on ESP32-P4 v1.3 (ECO2) silicon by explicitly defining `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` in `sdkconfig.defaults`.
- **Resilience Demo**: Validated the Degraded Mode. Cloud Transport successfully detects HTTP POST failures (due to missing SNTP/Certificates) and dynamically reroutes telemetry to the MicroSD Offline Spooler.
- **Companion ESP-NOW Receiver (C6)**: Implemented Wi-Fi STA mode initialization and ESP-NOW RX callback for capturing sensor broadcasts.
- **Companion IPC Sender (C6)**: Implemented COBS encoding, CRC16 hashing, and UART TX to forward ESP-NOW payloads to the Host.
- **Host IPC Ingestion (P4)**: Implemented Core 1 pinned FreeRTOS task with real-time DMA UART reads, COBS zero-allocation decoding, and CRC verification.
- **Direct mTLS to GCP**: Re-architected Cloud Transport to publish directly to Google Cloud Pub/Sub via HTTPS mTLS (Rust Backend acts only as a subscriber).
- **Phase 3 (MicroSD SDMMC VFS)**: Fully implemented `storage_manager` to mount FATFS on the Waveshare board's MicroSD slot, complete with hardware PMU LDO configuration.
- **Phase 4 (Security)**: Added `Gateway Security & Crypto` Kconfig and implemented a hybrid hardware/software ECDSA mechanism. Uses `ECDSA_DS` in production and a `dev_private_key.pem.dummy` fallback in CI/development.
- **Phase 5 (Offline Spooler)**: Implemented Store-and-Forward architecture via `offline_spooler`. If GCP connection fails, messages are appended to a binary file (`0xEDCE` framing + `CRC16-CCITT`) on the MicroSD.
- **Dynamic Identity Injection**: Gateway now injects `device_id` based on MAC address, relieving the sensor nodes from broadcasting their IDs over ESP-NOW.
- **Strict Governance**: Enforced Capa 2 GitHub Standard (Rulesets, strict checks, codeowners, and dependabot policies) across the workspace.
- Initial Project Scaffolding & Host-Companion Architecture definition.
- **Host firmware** (ESP32-P4): Ethernet pipeline with 7 modular components
  (`ipc_transport`, `telemetry_decoder`, `telemetry_buffer`, `cloud_transport`,
  `eth_manager`, `companion_ota`, `diagnostics`).
- **Companion firmware** (ESP32-C6): ESP-NOW Smart Proxy with 3 components
  (`espnow_receiver`, `ipc_sender`, `heartbeat`).
- **Shared components** (`cobs_crc`): COBS codec and CRC16-CCITT implementations
  referenced by both projects via `EXTRA_COMPONENT_DIRS`.
- **IPC frame protocol** (`ipc_frame.h`): 8 message types, 10-byte packed header,
  COBS framing with CRC16 integrity over UART at 460800 bps.
- **Kconfig**: 12 configurable parameters (GCP Pub/Sub endpoint, UART IPC pins,
  Ethernet PHY, companion control GPIOs, queue sizing).
- **Architecture Decision Records**: ADR-001 (Bypass ESP-Hosted), ADR-002
  (UART IPC over internal SDIO traces).
- **Repository governance**: LICENSE (Apache-2.0), CONTRIBUTING.md, SECURITY.md,
  CODEOWNERS, `.pre-commit-config.yaml` (clang-format 18).

[0.1.0]: https://github.com/oscar-bio-dev/edge-telemetry-gateway/releases/tag/v0.1.0
