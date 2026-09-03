# Edge Telemetry Gateway

> **Edge-to-Cloud telemetry hub** for ultra-low-power environmental monitoring networks.
> Receives ESP-NOW bursts from battery-powered sensor nodes and publishes to
> Google Cloud Pub/Sub over hardwired Ethernet with JWT/ECDSA hardware-accelerated authentication.

---

## Hardware Platform

**Board:** [Waveshare ESP32-P4-WIFI6-POE-ETH](https://www.waveshare.com/esp32-p4-wifi6-poe-eth.htm)

| SoC | Role | Key Capabilities |
|---|---|---|
| **ESP32-P4** (Host) | Edge Hub — data pipeline, crypto, cloud | Dual RISC-V 400MHz, 16MB PSRAM, EMAC+IP101GRI PHY (100Mbit Ethernet), AES/SHA/RSA/ECC/ECDSA\_DS HW accelerators, PoE powered |
| **ESP32-C6-MINI-1** (Companion) | RF Smart Proxy — ESP-NOW antenna | RISC-V 160MHz, Wi-Fi 6, native ESP-NOW, on-board SDIO/UART connection to P4 |

## System Architecture

```
                          ┌─ Shield: Waveshare ESP32-P4-WIFI6-POE-ETH ──────────────┐
                          │                                                          │
  Sensor Nodes            │  ESP32-C6 Companion        ESP32-P4 Host                │
  (room-monitoring)       │  ┌──────────────────┐      ┌──────────────────────────┐  │
  ┌──────────────┐        │  │                  │ UART │                          │  │
  │ BME688       │ ESP-NOW│  │  Wi-Fi STA       │460Kbps                         │  │  PoE
  │ SCD41        │────────┼─►│  ESP-NOW RX  ────┼──────►  COBS decode            │  │ Ethernet
  │ BMV080       │ burst  │  │  COBS encode     │ D0/D1│  Nanopb Protobuf decode │──┼──────► Google
  │ Protobuf     │  (ms)  │  │  CRC16 verify    │      │  JWT/ES256 (ECDSA_DS HW)│  │         Cloud
  │ Deep Sleep   │        │  │                  │      │  HTTPS → Pub/Sub        │  │        Pub/Sub
  └──────────────┘        │  └──────────────────┘      └──────────────────────────┘  │
                          │       ▲ GPIO20/21               │ EMAC + IP101GRI       │
                          │       │ (SDIO D0/D1 traces)     │ RMII 100Mbit/s        │
                          │       └─────────────────────────┘                        │
                          └──────────────────────────────────────────────────────────┘
```

### Data Flow

1. **Sensor nodes** (ESP32, `room-monitoring` project) wake from Deep Sleep, sample BME688/SCD41/BMV080, encode telemetry as **Nanopb Protobuf** (`EnvironmentalData`, ~50 bytes), and fire a millisecond ESP-NOW broadcast burst.

2. **C6 Companion** receives the ESP-NOW frame, wraps it in a **COBS-encoded IPC frame** (10-byte header: `type | src_mac[6] | seq_num | rssi` + Protobuf payload + CRC16-CCITT), and transmits over **UART at 460800 bps** through the board's internal SDIO D0/D1 traces (P4 GPIO14 ← C6 GPIO20, P4 GPIO15 → C6 GPIO21).

3. **P4 Host** decodes the COBS frame on Core 1 (`ipc_ingest_task`), verifies CRC16, decodes Protobuf with Nanopb (zero-allocation), and enqueues the sample into a static ring buffer (PSRAM-backed, 64 slots).

4. **Cloud uplink task** on Core 0 pops batches from the buffer, signs a **JWT (ES256)** using the P4's **ECDSA\_DS hardware accelerator** (private key in eFuse, never software-readable), and publishes to **Google Cloud Pub/Sub** via HTTPS over the native **Ethernet** (EMAC + IP101GRI RMII PHY, PoE powered).

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
│   ├── telemetry_buffer/             ← Ring buffer (zero-alloc, PSRAM)
│   ├── cloud_transport/              ← HTTPS + JWT/ECDSA → Pub/Sub
│   ├── eth_manager/                  ← EMAC + IP101GRI RMII + lwIP
│   ├── companion_ota/                ← Host-Driven OTA via esp-serial-flasher
│   └── diagnostics/                  ← Health checks, companion watchdog
│
├── companion/
│   ├── CMakeLists.txt                ← Companion project (target: esp32c6)
│   ├── main/                         ← ESP-NOW Smart Proxy boot
│   └── components/
│       ├── espnow_receiver/          ← Wi-Fi STA + ESP-NOW RX
│       ├── ipc_sender/               ← COBS encode + UART TX
│       └── heartbeat/                ← Alive signal to P4
│
├── shared_components/
│   └── cobs_crc/                     ← COBS codec + CRC16-CCITT (shared)
│
├── proto/telemetry.proto             ← Protobuf schema (shared with nodes)
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

## Configuration

All parameters are configurable via `idf.py menuconfig`:

| Parameter | Default | Description |
|---|---|---|
| `GCP_PROJECT_ID` | `setaesense-iot-core` | Google Cloud project |
| `GCP_PUB_SUB_TOPIC` | `room-telemetry-topic` | Pub/Sub topic name |
| `IPC_UART_BAUD_RATE` | `460800` | UART speed (Host ↔ Companion) |
| `ESPNOW_CHANNEL` | `1` | Wi-Fi channel (must match nodes) |
| `ETH_MDC_GPIO` / `ETH_MDIO_GPIO` | `31` / `52` | Ethernet PHY MDIO bus |
| `TELEMETRY_QUEUE_SIZE` | `64` | Ring buffer depth (samples) |
| `CLOUD_PUBLISH_BATCH_SIZE` | `10` | Samples per Pub/Sub request |
| `COMPANION_HEARTBEAT_TIMEOUT_MS` | `30000` | C6 watchdog timeout |

## Component Status

| Component | Status | Description |
|---|---|---|
| Project scaffolding | ✅ Complete | Dual-firmware structure, build system, Kconfig |
| COBS/CRC16 codec | ✅ Implemented | Zero-allocation, lookup-table CRC16 |
| IPC frame protocol | ✅ Defined | 8 message types, 10B header, COBS+CRC16 |
| IPC transport (UART) | 🔲 Stub | Core 1 ingest task pending |
| ESP-NOW receiver (C6) | 🔲 Stub | Wi-Fi STA + broadcast RX pending |
| Ethernet manager | 🔲 Stub | EMAC + IP101GRI + lwIP pending |
| Cloud transport | 🔲 Stub | JWT/ECDSA + Pub/Sub REST pending |
| Telemetry buffer | 🔲 Stub | PSRAM ring buffer pending |
| Host-Driven OTA | 🔲 Stub | esp-serial-flasher integration pending |
| Diagnostics | 🔲 Stub | Watchdog + health checks pending |

## Architecture Decisions

| ADR | Title | Status |
|---|---|---|
| [001](docs/adr/001-bypass-esp-hosted.md) | Bypass ESP-Hosted — C6 as dedicated ESP-NOW proxy | Accepted |
| [002](docs/adr/002-uart-ipc-over-sdio-traces.md) | UART IPC over internal SDIO D0/D1 traces with COBS | Accepted |

## License

Copyright 2026 oscar-bio-dev — [Apache License 2.0](LICENSE)
