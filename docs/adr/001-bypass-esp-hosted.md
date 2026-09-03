# ADR-001: Bypass ESP-Hosted — Direct ESP-NOW via Custom C6 Firmware

## Status: Accepted

## Context

The ESP32-P4-WIFI6-POE-ETH board connects an ESP32-P4 (Host) to an ESP32-C6-MINI-1
(Companion) via SDIO 4-bit bus. The standard approach is to use ESP-Hosted-MCU
to give the P4 WiFi/BT capabilities through the C6 coprocessor.

However, our sensor nodes transmit telemetry using **ESP-NOW** (connectionless
Wi-Fi protocol). Investigation confirmed that **ESP-Hosted-MCU does not support
ESP-NOW RPC callbacks** — the receive callback cannot be routed from the C6
coprocessor to the P4 host through the ESP-Hosted transport layer.

## Decision

**We bypass ESP-Hosted entirely.** Since the P4 has native Ethernet (EMAC +
IP101GRI PHY + PoE), it does not need WiFi for cloud connectivity.

The C6 runs a dedicated bare-metal ESP-IDF firmware that:
1. Initializes WiFi in STA mode on a configurable channel
2. Registers an ESP-NOW receive callback
3. Forwards received payloads to the P4 via UART (COBS-encoded)

The C6 acts as a pure "radio-to-serial bridge" (Smart Proxy).

## Consequences

- **Positive:** Eliminates the complexity of ESP-Hosted SDIO protocol, reduces
  C6 firmware to ~50KB, gives full control over the ESP-NOW reception path.
- **Negative:** WiFi fallback for cloud connectivity is not available unless
  ESP-Hosted is re-integrated in a future phase. Ethernet is the sole cloud path.
- **Risk:** If Ethernet fails, there is no network redundancy. Mitigated by
  the PSRAM ring buffer that retains data during outages.
