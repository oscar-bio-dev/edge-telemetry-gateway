# ADR-002: UART IPC over Internal SDIO Traces with COBS Framing

## Status: Accepted

## Context

The Waveshare ESP32-P4-WIFI6-POE-ETH board has 6 internal traces connecting
the P4 to the C6 module, originally designed for SDIO 4-bit communication:

| SDIO Signal | P4 GPIO | C6 GPIO | C6-MINI-1 Pin |
|---|---|---|---|
| CLK | GPIO18 | GPIO19 | Pin 25 |
| CMD | GPIO19 | GPIO18 | Pin 24 |
| D0  | GPIO14 | GPIO20 | Pin 26 |
| D1  | GPIO15 | GPIO21 | Pin 27 |
| D2  | GPIO16 | GPIO22 | Pin 28 |
| D3  | GPIO17 | GPIO23 | Pin 29 |

Since we bypass ESP-Hosted (ADR-001), we can repurpose these traces.

## Decision

**We configure 2 of the 6 traces as UART at 460800 bps:**
- P4 GPIO14 (RX) ← C6 GPIO20 (TX) — original SDIO D0 trace
- P4 GPIO15 (TX) → C6 GPIO21 (RX) — original SDIO D1 trace

Both the P4 (via GPIO Matrix) and C6 (via GPIO Matrix on UART1) support
arbitrary GPIO assignment for UART peripherals.

**Frame protocol: COBS + CRC16-CCITT**
- Deterministic overhead (max +1 byte per 254 payload bytes)
- 0x00 byte as unambiguous frame delimiter
- CRC16 for integrity verification

**Baud rate: 460800 bps** — provides 4× margin even if the R18/R19 resistors
(51KΩ) are in-series (worst-case RC = 0.51μs vs 2.2μs bit time).

## Consequences

- **Positive:** Simple driver (ESP-IDF UART), reuses existing PCB traces,
  H7 header remains free for debug, same UART path enables Host-Driven OTA.
- **Negative:** Lower throughput than SDIO (~46 KB/s vs ~4 MB/s). Irrelevant
  for our ~50-byte payloads at seconds-to-minutes intervals.
- **Risk:** If R18/R19 are truly 51KΩ series (not pull-ups), max reliable baud
  is ~500 Kbps. Verified adequate for our use case.
