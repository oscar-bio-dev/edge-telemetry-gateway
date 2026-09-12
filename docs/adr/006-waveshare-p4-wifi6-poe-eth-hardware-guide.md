# ADR-006: Waveshare ESP32-P4-WIFI6-POE-ETH — Technical Setup & Hardware Guide

- **Status:** Accepted / Active Architecture
- **Date:** 2026-09-11
- **Authors:** oscar-bio-dev, antigravity-ide
- **Board:** Waveshare ESP32-P4-WIFI6-POE-ETH (SKU: 28292)
- **Schematic Revision:** ESP32-P4-WIFI6-POE-ETH-Schematic.pdf (Waveshare)

## Summary

This document is the definitive hardware setup and technical guide for the Waveshare ESP32-P4-WIFI6-POE-ETH board. It documents the dual-chip ESP-NOW telemetry gateway architecture (ESP32-P4 Host + ESP32-C6 Companion), hardware limitations, required workarounds, and the **official reliable flashing procedure** for the C6 Companion chip using an external adapter.

This architecture is **Active and Highly Recommended** for high-performance edge computing requiring dedicated radio (C6) and dedicated cryptography/ethernet (P4).

---

## 1. Board Architecture

The Waveshare ESP32-P4-WIFI6-POE-ETH integrates two Espressif SoCs on a single PCB:

| SoC | Package | Role | Key Peripherals |
|---|---|---|---|
| **ESP32-P4** | QFN-74 (ECO2 rev 1.3) | Host MCU | Dual RISC-V 400MHz, 16MB PSRAM, EMAC + IP101GRI RMII PHY (100Mbit PoE Ethernet), ECDSA_DS HW, MicroSD (SDMMC 4-bit) |
| **ESP32-C6-MINI-1** | Module | Wi-Fi 6 Radio | RISC-V 160MHz, Wi-Fi 6, native ESP-NOW, BLE 5.0 |

### 1.1 Internal Inter-Chip Connection (SDIO Bus)

The board connects the P4 to the C6 module through **6 internal PCB traces** originally designed for SDIO 4-bit communication. These traces are the **only** electrical connection between the two chips (apart from shared ground and power rails):

| Signal | P4 GPIO | C6 GPIO | C6-MINI-1 Pin | Series Resistor |
|---|---|---|---|---|
| SDIO CLK | GPIO18 | GPIO19 | Pin 25 | — |
| SDIO CMD | GPIO19 | GPIO18 | Pin 24 | — |
| SDIO D0 | GPIO14 | GPIO20 | Pin 26 | R19 (51KΩ) |
| SDIO D1 | GPIO15 | GPIO21 | Pin 27 | R18 (51KΩ) |
| SDIO D2 | GPIO16 | GPIO22 | Pin 28 | — |
| SDIO D3 | GPIO17 | GPIO23 | Pin 29 | — |

We repurposed SDIO D0 and D1 as UART lines (see ADR-002) to form an IPC (Inter-Process Communication) bridge:
- **P4 GPIO14 (RX) ← C6 GPIO20 (TX)** — SDIO D0 trace through R19
- **P4 GPIO15 (TX) → C6 GPIO21 (RX)** — SDIO D1 trace through R18

---

## 2. Definitive Guide: Flashing the ESP32-C6 Companion

The board exposes a Type-C port that is **exclusively connected to the ESP32-P4**. There is no direct USB connection from the host PC to the ESP32-C6. To interact with the C6 (e.g., flashing firmware), an external USB-to-UART adapter is strictly required.

> [!WARNING]
> The C6 UART lines (GPIO20/GPIO21) are electrically shared with the P4 GPIO14/GPIO15. If the P4 is executing user code (e.g., `ipc_transport`), its output will collide with the external adapter's signals, corrupting the flash protocol and throwing `Serial data stream stopped` errors.

### 2.1 Required Equipment
- An external 3.3V TTL USB-to-UART adapter (e.g., **DFRobot RainbowLink V2** based on CH343).
- Jumper wires.

### 2.2 Physical Connection (ESP32-C6 UART Terminal)
Locate the 6-pin terminal on the board labeled `ESP32-C6 UART`. Connect it to your TTL adapter as follows:

| C6 Terminal Pin | DFRobot TTL Adapter |
|---|---|
| `TXD` (C6_U0TXD) | `RX` |
| `RXD` (C6_U0RXD) | `TX` |
| `GND` | `GND` |
| `IO9` | Temporarily short to `GND` before power-on |

### 2.3 The Flashing Procedure (Step-by-Step)
To successfully flash the C6, we must force the P4 to stay in its ROM bootloader, preventing it from initializing its UART drivers and injecting noise into the bus.

1. Power **OFF** the board (disconnect Type-C and PoE).
2. Ensure the TTL adapter is connected as detailed above, including the **IO9 to GND** short.
3. **Press and hold the `BOOT` button** on the Waveshare board (this is the P4's boot button).
4. While holding the `BOOT` button, plug in the Type-C cable to power on the board.
5. You can now release the `BOOT` button. The P4 is safely trapped in ROM download mode.
6. The C6 is now in download mode (due to IO9 being low at boot).
7. Execute the flash command targeting the TTL adapter's COM port (e.g., `/dev/ttyACM5`):
   ```bash
   cd companion
   idf.py -p /dev/ttyACM5 flash monitor
   ```
8. Remove the IO9-to-GND short and press the `RST` button to boot both chips normally.

---

## 3. Technical Hurdles Overcome (Chronology)

During the development of this dual-chip architecture, several technical challenges were encountered and successfully mitigated. They are documented here for future reference.

### 3.1 ESP32-P4 ECO2 Illegal Instruction Panic
- **Symptom:** Immediate `Guru Meditation Error: Core 0 panic'ed (Illegal instruction)` in the bootloader upon first power-on.
- **Root Cause:** The Waveshare board carries ESP32-P4 ECO2 silicon (revision 1.3). The default ESP-IDF toolchain emits RISC-V instructions only valid for silicon rev ≥ 3.
- **Resolution:** Added mitigation flags to `sdkconfig.defaults`:
  ```ini
  CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
  CONFIG_ESP32P4_REV_MIN_100=y
  ```

### 3.2 C6 Companion Wi-Fi Crash Loop
- **Symptom:** C6 firmware entered infinite reboot loop with `ESP_ERR_WIFI_NOT_STARTED`.
- **Root Cause:** The ESP-IDF Wi-Fi driver state machine requires `esp_wifi_start()` to complete **before** any channel configuration.
- **Resolution:** Reordered initialization sequence: `esp_wifi_start()` -> `esp_wifi_set_channel()` -> `esp_now_init()`.

### 3.3 UART Data Collision (The "Flasher" Issue)
- **Symptom:** `esptool` consistently failed with `Serial data stream stopped: Possible serial noise or corruption` when trying to flash the C6.
- **Root Cause:** The P4 was booting into user code simultaneously, driving GPIO14/15 and electrically colliding with the DFRobot TTL adapter on the shared traces.
- **Resolution:** Documented and implemented the physical BOOT-button strapping procedure (Section 2.3) which perfectly isolates the C6 lines.

---

## 4. Board Verdict & Architectural Strategy

| Criterion | Verdict | Notes |
|---|---|---|
| P4 Host development | ✅ Excellent | 400MHz dual-core, PoE Ethernet, MicroSD, ECDSA_DS |
| P4 ↔ C6 UART IPC | ✅ Stable & Verified | Works flawlessly at 460800 bps through 51KΩ series resistors using COBS framing. |
| C6 Flashing | ✅ Reliable | Works perfectly using the BOOT-button hardware isolation procedure. |
| Production deployment | ✅ Recommended | Highly stable dual-chip architecture. The C6 codebase is strictly frozen to eliminate field-update risks. |

### Strategic Roadmap
This project (`edge-telemetry-gateway`) serves as our flagship high-performance Gateway utilizing the **P4+C6 dual-chip** topology. 

In parallel, we are developing the `esp32s3-edge-gateway` project (based on ESP32-S3 + W5500 SPI Ethernet). The S3 project is **NOT** a replacement for this P4 architecture, but rather an ongoing research effort to provide a benchmark for **efficiency, power consumption, and scalability comparison** between the two paradigms (Dual-SoC vs Single-SoC + External PHY).

## 5. References

- Waveshare Product Page: https://www.waveshare.com/esp32-p4-wifi6-poe-eth.htm
- ESP32-P4 Technical Reference Manual (Espressif)
- ESP32-C6-MINI-1 Module Datasheet (Espressif)
- DFRobot RainbowLink V2: https://wiki.dfrobot.com/tel0190
- ADR-001: Bypass ESP-Hosted
- ADR-002: UART IPC over SDIO traces
- ADR-003: ESP32-P4 ECO2 rev 1.3 silicon workarounds
