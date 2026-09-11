# Edge Telemetry Gateway (Deprecated Archive)

> [!CAUTION]
> **DEPRECATED**: This dual-chip architecture (ESP32-P4 + C6) is no longer under active development and serves as an **archival/historical reference**.
> Due to critical hardware defects on the Waveshare P4 board (unresolvable SDIO/UART pin multiplexing conflicts causing firmware lockups on the C6), this project has been migrated to a single-chip **ESP32-S3** architecture.
>
> Please use the new repository: [`edge-s3-gateway`](../edge-s3-gateway)

> **Edge-to-Cloud telemetry hub** for ultra-low-power environmental monitoring networks.
> Receives ESP-NOW bursts from battery-powered sensor nodes and publishes to
> Google Cloud Pub/Sub via HTTPS mTLS with JWT/ECDSA hardware-accelerated authentication.

> [!WARNING]
> **Board Status: Waveshare ESP32-P4-WIFI6-POE-ETH**
> This repository was developed on the Waveshare ESP32-P4-WIFI6-POE-ETH board.
> Development has been **frozen** due to irrecoverable C6 flashing instability
> caused by the board's shared CH344Q USB hub architecture. See
> [ADR-006](docs/adr/006-waveshare-p4-wifi6-poe-eth-post-mortem.md) for the
> full post-mortem. Active development has moved to
> [`edge-s3-gateway`](https://github.com/oscar-bio-dev/edge-s3-gateway)
> (ESP32-S3 + W5500). The P4 firmware will be revived when the
> ESP32-P4X-Function-EV-Board arrives (~Q1 2027).

---

## Hardware Platform

**Board:** [Waveshare ESP32-P4-WIFI6-POE-ETH](https://www.waveshare.com/esp32-p4-wifi6-poe-eth.htm)

| SoC | Role | Key Capabilities |
|---|---|---|
| **ESP32-P4** (Host) | Edge Hub — data pipeline, crypto, cloud | Dual RISC-V 400MHz, 16MB PSRAM, EMAC+IP101GRI PHY (100Mbit Ethernet), AES/SHA/RSA/ECC/ECDSA\_DS HW accelerators, PoE powered |
| **ESP32-C6-MINI-1** (Companion) | RF Smart Proxy — ESP-NOW antenna | RISC-V 160MHz, Wi-Fi 6, native ESP-NOW, on-board SDIO/UART connection to P4 |

### Known Hardware Issues

> [!CAUTION]
> The Waveshare ESP32-P4-WIFI6-POE-ETH has **critical limitations** for
> dual-chip development. Read [ADR-006](docs/adr/006-waveshare-p4-wifi6-poe-eth-post-mortem.md)
> before investing time in this board.

| Issue | Severity | Details |
|---|---|---|
| C6 USB flashing via shared CH344Q hub | 🔴 **Critical** | `esptool` fails non-deterministically with `Serial data stream stopped`. The C6's USB-Serial/JTAG peripheral shares a quad-port USB hub with the P4. Hub re-enumeration races corrupt the SLIP protocol. |
| P4 → C6 EN/BOOT GPIO control | 🟡 **Major** | GPIO54 (EN) and GPIO6 (BOOT) do not reliably toggle the C6's strapping pins. Host-Driven OTA via `esp-serial-flasher` is not feasible. |
| UART collision on shared SDIO traces | 🟡 **Major** | P4 GPIO14/15 (IPC UART) share PCB traces with C6 GPIO20/21. P4 must be frozen before flashing C6 via H7 header. |
| ESP32-P4 ECO2 rev 1.3 silicon | 🟢 **Resolved** | Requires `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` in `sdkconfig.defaults`. See [ADR-003](docs/adr/003-esp32p4-eco2-rev13-workarounds.md). |

## System Architecture

```
                          ┌─ Shield: Waveshare ESP32-P4-WIFI6-POE-ETH ──────────────┐
                          │                                                          │
  Sensor Nodes            │  ESP32-C6 Companion        ESP32-P4 Host                │
  (room-monitoring)       │  ┌──────────────────┐      ┌──────────────────────────┐  │  PoE
  ┌──────────────┐        │  │                  │ UART │                          │  │  Ethernet
  │ BME688       │ ESP-NOW│  │  Wi-Fi STA       │460Kbps                          │  │  HTTPS mTLS
  │ SCD41        │────────┼─►│  ESP-NOW RX  ────┼──────►  COBS / Nanopb decode   │──┼──────► Rust
  │ BMV080       │ burst  │  │  COBS encode     │ D0/D1│  JWT/ES256 (ECDSA_DS)   │  │        Backend
  │ Protobuf     │  (ms)  │  │  CRC16 verify    │      │                          │  │
  │ Deep Sleep   │        │  │                  │      └──┬───────────────────────┘  │
  └──────────────┘        │  └──────────────────┘         │ SDMMC (4-bit)            │
                          │       ▲ GPIO20/21             ▼                          │
                          │       │ (SDIO D0/D1)       ┌─────────────────┐           │
                          │       └────────────────────┤ MicroSD (VFS)   │           │
                          │                            │ Spooler & ESP-DL│           │
                          │                            └─────────────────┘           │
                          └──────────────────────────────────────────────────────────┘
```

### Data Flow

1. **Sensor nodes** (ESP32, `room-monitoring` project) wake from Deep Sleep, sample BME688/SCD41/BMV080, encode telemetry as **Nanopb Protobuf** (`EnvironmentalData`, ~50 bytes), and fire a millisecond ESP-NOW broadcast burst.

2. **C6 Companion** receives the ESP-NOW frame, wraps it in a **COBS-encoded IPC frame** (10-byte header: `type | src_mac[6] | seq_num | rssi` + Protobuf payload + CRC16-CCITT), and transmits over **UART at 460800 bps** through the board's internal SDIO D0/D1 traces (P4 GPIO14 ← C6 GPIO20, P4 GPIO15 → C6 GPIO21).

3. **P4 Host** decodes the COBS frame on Core 1 (`ipc_ingest_task`), verifies CRC16, decodes Protobuf with Nanopb (zero-allocation), injects the node's MAC as the `device_id`, and enqueues the sample into a static ring buffer.

4. **Cloud uplink task** on Core 0 pops batches from the buffer, signs a **JWT (ES256)** using the P4's **ECDSA\_DS hardware accelerator** (private key in eFuse), and publishes to **Google Cloud Pub/Sub** via **HTTPS mTLS** over native Ethernet (EMAC + IP101GRI RMII PHY, PoE powered). The decoupled Rust Backend serves exclusively as a subscriber to the GCP topics.

### Why Not ESP-Hosted?

The standard approach for giving the P4 Wi-Fi is ESP-Hosted-MCU (C6 as SDIO co-processor). However, **ESP-Hosted does not support ESP-NOW RPC callbacks** — the receive path cannot be routed to the Host. Since the P4 has native Ethernet for cloud connectivity, we bypass ESP-Hosted entirely and run a dedicated bare-metal firmware on the C6 that acts as a pure radio-to-serial bridge. See [ADR-001](docs/adr/001-bypass-esp-hosted.md).

## Project Structure

This repository contains **two independent ESP-IDF projects** under a single git tree:

```
edge-telemetry-gateway/
├── CMakeLists.txt                    ← Host project (target: esp32p4)
├── main/                             ← Orchestration & boot
├── components/
│   ├── ipc_transport/                ← UART RX + COBS decode (Core 1)
│   ├── telemetry_decoder/            ← Nanopb static decode
│   ├── telemetry_buffer/             ← Ring buffer (zero-alloc, soon to be SPIFFS-backed)
│   ├── cloud_transport/              ← HTTPS mTLS + JWT/ECDSA → Backend
│   ├── eth_manager/                  ← EMAC + IP101GRI RMII + lwIP
│   ├── companion_ota/                ← Host-Driven OTA via esp-serial-flasher
│   ├── cli_manager/                  ← ESP Console CLI for peer provisioning
│   └── diagnostics/                  ← Health checks, companion watchdog
│
├── companion/
│   ├── CMakeLists.txt                ← Companion project (target: esp32c6)
│   ├── main/                         ← ESP-NOW Smart Proxy boot
│   ├── flash_c6.sh                   ← Flash C6 via USB (unreliable, see ADR-006)
│   ├── flash_c6_slow.sh              ← Baud-sweep flash script (9600–74880)
│   └── components/
│       ├── espnow_receiver/          ← Wi-Fi STA + ESP-NOW RX
│       ├── ipc_sender/               ← COBS encode + UART TX
│       └── heartbeat/                ← Alive signal to P4
│
├── shared_components/
│   ├── cobs_crc/                     ← COBS codec + CRC16-CCITT (shared)
│   ├── proto/telemetry.proto         ← Single Source of Truth Protobuf schema
│   └── proto/gateway_health.proto    ← Diagnostics & Degraded Mode Protobuf schema
├── docs/adr/                         ← Architecture Decision Records
└── scripts/flash_companion.sh        ← Flash C6 via H7 debug header
```

Both projects reference `shared_components/` via `EXTRA_COMPONENT_DIRS` in their
root `CMakeLists.txt`, ensuring a single source of truth for the IPC protocol.

## Quick Start

```bash
# Source ESP-IDF environment
. $IDF_PATH/export.sh

# ── Build & Flash Host (ESP32-P4) ──
idf.py set-target esp32p4
idf.py build flash monitor

# ── Build & Flash Companion (ESP32-C6) ──
cd companion/
idf.py set-target esp32c6
idf.py build
# Flash via H7 debug header (default port /dev/ttyUSB1)
../scripts/flash_companion.sh /dev/ttyUSB1
```

> [!WARNING]
> **HARDWARE DEPRECATION NOTICE (Sep 10, 2026)**
> The Waveshare ESP32-P4-WIFI6-POE-ETH board has been temporarily abandoned for this project.
> We discovered a fatal hardware design flaw: the C6 module cannot be flashed once the P4 is programmed. The board lacks an internal USB connection for the C6, the H7 debug header has no BOOT pin exposed, and host-driven strapping via P4 GPIOs is electrically unstable resulting in permanent "soft-bricks" for updates.
> See [ADR-006](../ESP32-P4-WIFI6-POE-ETH/REPORTE_SITUACION.md) for the complete post-mortem.
> Development will continue on an **ESP32-S3** board until the official `ESP32-P4X-Function-EV-Board` is delivered.

## Configuration

All parameters are configurable via `idf.py menuconfig`:

| Parameter | Default | Description |
|---|---|---|
| `GCP_PUBSUB_ENDPOINT` | `http://.../topics/room-telemetry:publish` | Pub/Sub Endpoint (Production or Emulator) |
| `ENABLE_MOCK_TELEMETRY` | `y` (Lab mode) | Injects dummy data into RAM queue |
| `IPC_UART_BAUD_RATE` | `460800` | UART speed (Host ↔ Companion) |
| `ESPNOW_CHANNEL` | `1` | Wi-Fi channel (must match nodes) |
| `ETH_MDC_GPIO` / `ETH_MDIO_GPIO` | `31` / `52` | Ethernet PHY MDIO bus |
| `TELEMETRY_QUEUE_SIZE` | `64` | Ring buffer depth (samples) |
| `CLOUD_PUBLISH_BATCH_SIZE` | `10` | Samples per HTTPS request |
| `COMPANION_HEARTBEAT_TIMEOUT_MS` | `30000` | C6 watchdog timeout |

## Lab Testing (Test 1 - Validated)

The Gateway features a built-in isolated mock injector to validate the End-to-End architecture (Queue -> JWT -> TLS -> Pub/Sub) without needing physical sensors.
**Status:** 100% Validated with the Rust backend consuming from the GCP Emulator.

1. Ensure the Rust server's GCP Emulator is running on the local network (`0.0.0.0:8085`).
2. Run `idf.py menuconfig`.
3. Under **Development & Lab Testing**, enable `Enable Mock Telemetry Injector`.
4. Under **Edge Telemetry Gateway Configuration**, ensure the `Google Cloud Pub/Sub Endpoint URL` points to your local emulator IP aligning with the backend project and topic (e.g. `http://192.168.0.32:8085/v1/projects/oscar-bio-dev-project/topics/room-telemetry:publish`).
5. Build and flash. The Gateway will inject dummy Protobufs every 5 seconds, sign JWTs with MbedTLS PSA Crypto, and publish them to the emulator.

> **Warning:** NEVER enable `ENABLE_MOCK_TELEMETRY` in production firmware.

## Physical Integration (Test 2 - Validated)

Phase 2 replaces the mock injector with the **ESP32-C6 Companion Proxy** receiving real encrypted telemetry over **ESP-NOW**.
**Status:** 100% Validated. Hardware UART collisions between P4 and C6 were resolved, and the Wi-Fi driver state machine was stabilized.

1. Flash the ESP32-C6 (Companion) while keeping the P4 physical UART lines released.
2. Obtain the base MAC address from the C6 logs (e.g., `B0:A6:04:9A:15:F8`).
3. Inject the C6 MAC address, PMK, and LMK into the Sensor Node (`room-monitoring`) via Kconfig for secure CCMP-128 peer-to-peer encryption.
4. The C6 receives the sensor broadcast, encapsulates it via COBS/CRC16, and sends it to the P4 Host for immediate uplink to the Cloud via Ethernet.

## Component Status

| Component | Status | Description |
|---|---|---|
| Project scaffolding | ✅ Complete | Dual-firmware structure, build system, Kconfig |
| COBS/CRC16 codec | ✅ Implemented | Zero-allocation, lookup-table CRC16 |
| IPC frame protocol | ✅ Defined | 8 message types, 10B header, COBS+CRC16 |
| IPC transport (UART) | ✅ Implemented | Core 1 ingest task, COBS TX/RX |
| ESP-NOW receiver (C6) | ✅ Implemented | Wi-Fi STA + broadcast RX + MAC extraction |
| Ethernet manager | ✅ Implemented | EMAC + IP101GRI driver configured |
| Cloud transport | ✅ Implemented | HTTPS mTLS to GCP Pub/Sub + JWT/ECDSA Auth |
| Storage & Spooler | ✅ Implemented | MicroSD (SDMMC VFS) Store-and-Forward |
| CLI Manager | ✅ Implemented | ESP Console REPL for dynamic peer provisioning |
| Edge AI (ESP-DL) | 🔲 Stub | Neural Network inference on historical telemetry |
| Host-Driven OTA | ❌ Blocked | esp-serial-flasher blocked by GPIO control issue (ADR-006) |
| Diagnostics | ✅ Implemented | Watchdog + Degraded Mode health checks (Auto-Spooler Rerouting) |

## Architecture Decisions

| ADR | Title | Status |
|---|---|---|
| [001](docs/adr/001-bypass-esp-hosted.md) | Bypass ESP-Hosted — C6 as dedicated ESP-NOW proxy | Accepted |
| [002](docs/adr/002-uart-ipc-over-sdio-traces.md) | UART IPC over internal SDIO D0/D1 traces with COBS | Accepted |
| [003](docs/adr/003-esp32p4-eco2-rev13-workarounds.md) | ESP32-P4 ECO2 Rev 1.3 Silicon Errata Workarounds | Accepted |
| [004](docs/adr/004-mbedtls-cross-version-compat.md) | MbedTLS Cross-Version Compatibility | Accepted |
| [005](docs/adr/005-microsd-vfs-and-degraded-mode.md) | MicroSD SDMMC VFS & Degraded Mode | Accepted |
| [006](docs/adr/006-waveshare-p4-wifi6-poe-eth-post-mortem.md) | **Waveshare P4-WIFI6-POE-ETH Post-Mortem** | Accepted |

## Security & Crypto

The Gateway utilizes a hybrid hardware/software ECDSA mechanism. The P4's hardware `ECDSA_DS` peripheral is used to sign JWTs utilizing an eFuse-burned private key.

For development and CI environments, a software fallback is provided via a dummy key (`dev_private_key.pem.dummy`). CMake automatically copies and embeds this key into the `cloud_transport` component if the real `.pem` is missing, ensuring CI pipelines do not break while preserving strict `.gitignore` rules for actual key material.

## License

Copyright 2026 oscar-bio-dev — [Apache License 2.0](LICENSE)
