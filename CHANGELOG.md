# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - Unreleased

### Added
- **Companion ESP-NOW Receiver (C6)**: Implemented Wi-Fi STA mode initialization and ESP-NOW RX callback for capturing sensor broadcasts.
- **Companion IPC Sender (C6)**: Implemented COBS encoding, CRC16 hashing, and UART TX to forward ESP-NOW payloads to the Host.
- **Host IPC Ingestion (P4)**: Implemented Core 1 pinned FreeRTOS task with real-time DMA UART reads, COBS zero-allocation decoding, and CRC verification.
- **Direct mTLS Cloud Routing**: Replaced Google Cloud Pub/Sub architecture with direct HTTPS mTLS routing to the Rust backend to prevent schema drift and eliminate intermediate infrastructure.
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
