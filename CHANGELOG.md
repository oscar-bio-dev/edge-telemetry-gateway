# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - Unreleased

### Added
- **ESP-IDF v6.1 Migration**: Successfully migrated to v6.1. Fixed RISC-V Illegal Instruction panics on ESP32-P4 v1.3 (ECO2) silicon by explicitly defining `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` in `sdkconfig.defaults`.
- **MbedTLS 3 (PSA Crypto)**: Upgraded ECDSA JWT signing to use PSA Crypto APIs (`psa_sign_message`), deprecating legacy `mbedtls_pk_sign` entropy injection.
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
