# ADR-003: ESP32-P4 ECO2 Rev 1.3 — Silicon Errata Workarounds

- **Status:** Accepted
- **Date:** 2026-09-04
- **Context:** Waveshare ESP32-P4-WIFI6-POE-ETH board carries ESP32-P4 ECO2 silicon, revision 1.3.

## Problem

Upon first boot, the ESP32-P4 immediately panics in the bootloader with:

```
Guru Meditation Error: Core 0 panic'ed (Illegal instruction)
PC: 0x4ffab2ca — call_start_cpu0 at bootloader_start.c:27
```

The bootloader compiled by ESP-IDF's default toolchain settings emits RISC-V instructions that are only valid for silicon rev ≥ 3 (v3.0). The ECO2 rev 1.3 silicon does not implement these instructions, causing an immediate trap.

## Decision

Add the following mandatory flags to `sdkconfig.defaults` (shared by all build configurations):

```ini
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
CONFIG_ESP32P4_REV_MIN_100=y
```

These flags instruct the ESP-IDF build system to:
1. Emit only instructions compatible with silicon rev < 3.
2. Declare rev 1.0 (100) as the minimum supported revision, which covers our 1.3 silicon.

## Consequences

- **Positive:** The bootloader and application boot successfully on ECO2 rev 1.3 hardware.
- **Positive:** Forward-compatible — when rev ≥ 3 silicon becomes available, the flags can be removed to unlock newer instruction set extensions.
- **Negative:** Some potential micro-optimizations available only on rev ≥ 3 are unavailable.
- **Risk:** If these flags are accidentally removed from `sdkconfig.defaults`, the firmware will brick on boot with no recovery possible except re-flashing with correct flags.

## Validation

- Confirmed via `idf.py build flash monitor` on physical Waveshare board (2026-09-04).
- Confirmed via ESP-IDF boot log: `rst:0x1 (POWERON),boot:0x30f (SPI_FAST_FLASH_BOOT)` followed by successful `app_main()` entry.
- Cross-referenced with `esp-pilot-mcp/catalog_list_socs` which lists `esp32p4` as a supported target.
