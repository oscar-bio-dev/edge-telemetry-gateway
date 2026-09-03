# Edge Telemetry Gateway

> Unified Edge Hub for environmental monitoring — receives ESP-NOW telemetry
> from battery-powered sensor nodes and publishes to Google Cloud Pub/Sub
> over Ethernet with JWT/ECDSA authentication.

## Architecture

```
Sensor Nodes              ESP32-C6 Companion         ESP32-P4 Host
(room-monitoring)         "Smart Proxy"              "Edge Hub"
┌──────────────┐          ┌──────────────────┐       ┌─────────────────────────┐
│ BME688/SCD41 │ ESP-NOW  │ ESP-NOW RX       │ UART  │ COBS decode + Protobuf  │
│ BMV080       │─────────►│ COBS encode      │──────►│ JWT/ECDSA (HW accel)    │
│ Protobuf     │ burst ms │ CRC16            │460Kbps│ HTTPS → GCP Pub/Sub     │
│ Deep Sleep   │          │                  │       │ Ethernet (IP101 RMII)   │
└──────────────┘          └──────────────────┘       └─────────────────────────┘
```

**Hardware:** ESP32-P4-WIFI6-POE-ETH (Waveshare) — dual RISC-V 400MHz,
16MB PSRAM, Ethernet PoE, on-board ESP32-C6-MINI-1 companion.

## Project Structure

This repository contains **two separate ESP-IDF projects**:

| Project | Target | Path | Purpose |
|---|---|---|---|
| **Host** | ESP32-P4 | `/` (root) | Cloud gateway, Ethernet, TLS, data pipeline |
| **Companion** | ESP32-C6 | `/companion/` | ESP-NOW receiver, UART bridge to Host |

Shared code (COBS codec, CRC16, IPC frame definitions) lives in
`/shared_components/` and is referenced by both projects via `EXTRA_COMPONENT_DIRS`.

## Quick Start

```bash
# Source ESP-IDF environment
. $IDF_PATH/export.sh

# Build and flash Host (ESP32-P4)
idf.py set-target esp32p4
idf.py build flash monitor

# Build and flash Companion (ESP32-C6) — from companion/ directory
cd companion/
idf.py set-target esp32c6
idf.py build
# Flash via H7 debug header or Host-Driven OTA
```

## Configuration

Use `idf.py menuconfig` to adjust:
- **GCP Project ID / Pub/Sub Topic** — Cloud endpoint
- **IPC UART Baud Rate** — Inter-chip communication speed (default: 460800)
- **ESP-NOW Channel** — Radio channel matching sensor nodes (default: 1)
- **Ethernet PHY** — IP101GRI address and GPIO mapping

## Status

| Component | Status |
|---|---|
| Project scaffolding | ✅ Complete |
| COBS/CRC16 codec | ✅ Implemented |
| IPC frame protocol | ✅ Defined |
| IPC transport (UART) | 🔲 Stub |
| ESP-NOW receiver (C6) | 🔲 Stub |
| Ethernet manager | 🔲 Stub |
| Cloud transport (JWT/TLS) | 🔲 Stub |
| Telemetry buffer | 🔲 Stub |
| Host-Driven OTA | 🔲 Stub |
| Diagnostics | 🔲 Stub |

## License

Apache-2.0 — See [LICENSE](LICENSE)
