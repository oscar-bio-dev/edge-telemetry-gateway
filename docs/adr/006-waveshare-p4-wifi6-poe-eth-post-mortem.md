# ADR-006: Waveshare ESP32-P4-WIFI6-POE-ETH — Hardware Errata & Post-Mortem

- **Status:** Accepted
- **Date:** 2026-09-09
- **Authors:** oscar-bio-dev, antigravity-ide
- **Board:** Waveshare ESP32-P4-WIFI6-POE-ETH (SKU: 28292)
- **Schematic Revision:** ESP32-P4-WIFI6-POE-ETH-Schematic.pdf (Waveshare)

## Summary

This document is a comprehensive post-mortem of 6 days of development on the
Waveshare ESP32-P4-WIFI6-POE-ETH board. It documents every hardware limitation,
workaround, and failure mode encountered while building a dual-chip ESP-NOW
telemetry gateway (ESP32-P4 Host + ESP32-C6 Companion). **This board was
ultimately abandoned** in favor of the ESP32-P4X-Function-EV-Board (Espressif
official) due to irrecoverable C6 flashing instability caused by the board's
shared USB/UART hub architecture.

If you are considering this board for a production project, **read this document
in its entirety** before committing.

---

## 1. Board Architecture

The Waveshare ESP32-P4-WIFI6-POE-ETH integrates two Espressif SoCs on a single
PCB:

| SoC | Package | Role | Key Peripherals |
|---|---|---|---|
| **ESP32-P4** | QFN-74 (ECO2 rev 1.3) | Host MCU | Dual RISC-V 400MHz, 16MB PSRAM, EMAC + IP101GRI RMII PHY (100Mbit PoE Ethernet), ECDSA_DS HW, MicroSD (SDMMC 4-bit) |
| **ESP32-C6-MINI-1** | Module | Wi-Fi 6 Radio | RISC-V 160MHz, Wi-Fi 6, native ESP-NOW, BLE 5.0 |

### 1.1 Internal Inter-Chip Connection (SDIO Bus)

The board connects the P4 to the C6 module through **6 internal PCB traces**
originally designed for SDIO 4-bit communication. These traces are the **only**
electrical connection between the two chips (apart from shared ground and power
rails):

| Signal | P4 GPIO | C6 GPIO | C6-MINI-1 Pin | Series Resistor |
|---|---|---|---|---|
| SDIO CLK | GPIO18 | GPIO19 | Pin 25 | — |
| SDIO CMD | GPIO19 | GPIO18 | Pin 24 | — |
| SDIO D0 | GPIO14 | GPIO20 | Pin 26 | R19 (51KΩ) |
| SDIO D1 | GPIO15 | GPIO21 | Pin 27 | R18 (51KΩ) |
| SDIO D2 | GPIO16 | GPIO22 | Pin 28 | — |
| SDIO D3 | GPIO17 | GPIO23 | Pin 29 | — |

We repurposed SDIO D0 and D1 as UART lines (see ADR-002):
- **P4 GPIO14 (RX) ← C6 GPIO20 (TX)** — SDIO D0 trace through R19
- **P4 GPIO15 (TX) → C6 GPIO21 (RX)** — SDIO D1 trace through R18

### 1.2 USB Hub Architecture (Critical — Root Cause of All Flashing Failures)

Both the ESP32-P4 and ESP32-C6 share a **single USB connection to the host PC**
through an on-board USB hub chip (WCH CH344Q, Quad Serial). When connected via
USB-C, the hub enumerates as **four** `ttyACMx` devices:

```
usb-wch.cn_USB_Quad_Serial_0123456789-if00 -> /dev/ttyACM2  (P4 JTAG)
usb-wch.cn_USB_Quad_Serial_0123456789-if02 -> /dev/ttyACM3  (P4 UART0)
usb-wch.cn_USB_Quad_Serial_0123456789-if04 -> /dev/ttyACM4  (C6 JTAG)
usb-wch.cn_USB_Quad_Serial_0123456789-if06 -> /dev/ttyACM5  (C6 USB Serial/JTAG)
```

**Critical implication:** When `esptool` attempts to flash the C6 via
`/dev/ttyACM5`, it sends DTR/RTS reset signals through the CH344Q hub. The hub
routes these signals to the C6's USB-Serial/JTAG peripheral, but the reset
causes the C6's internal USB CDC stack to momentarily disconnect. Since the USB
connection goes through the **shared** hub (not a dedicated USB port), this
creates a race condition where the Linux kernel may re-enumerate the device,
drop the serial connection, or the hub itself may inject noise into the data
lines.

### 1.3 C6 Control GPIOs (Partially Functional)

The schematic shows the following control connections between P4 and C6:

| Function | P4 GPIO | C6 Pin | Resistor | Observed Behavior |
|---|---|---|---|---|
| C6 EN (Reset) | GPIO54 | CHIP_PU | R54 (0Ω) | **Unreliable.** Toggle did not produce observable reset on C6. Boot log showed `POWERON` reset type even after P4 toggled this GPIO. |
| C6 BOOT (IO9 strapping) | GPIO6 | IO9 | R52 (0Ω) | **Not observed.** P4 GPIO6 toggle did not force C6 into download mode. Manual IO9-to-GND jumper wire was required. |

### 1.4 H7 Debug Header

The board exposes a 6-pin header (H7) with direct access to the C6's UART0
lines. This is the only reliable path for flashing the C6 when using an
**external** USB-UART adapter (e.g., RainbowLink V2, FTDI):

| H7 Pin | Signal | C6 GPIO |
|---|---|---|
| 1 | GND | — |
| 2 | VCC (3.3V) | — |
| 3 | C6_U0TXD | GPIO20 |
| 4 | C6_U0RXD | GPIO21 |
| 5 | C6 EN | CHIP_PU |
| 6 | C6 BOOT | IO9 |

**However**, pins 3 and 4 (UART) are **shared** with the internal SDIO D0/D1
traces to the P4. If the P4 is actively driving GPIO14/GPIO15 (e.g., running
`ipc_transport`), its output will collide with the external adapter's signals,
corrupting all data.

---

## 2. Problems Encountered (Chronological)

### 2.1 ESP32-P4 ECO2 Illegal Instruction Panic (Day 1 — Resolved)

**Symptom:** Immediate `Guru Meditation Error: Core 0 panic'ed (Illegal
instruction)` in the bootloader upon first power-on.

**Root Cause:** The Waveshare board carries ESP32-P4 ECO2 silicon (revision 1.3).
The default ESP-IDF toolchain emits RISC-V instructions only valid for silicon
rev ≥ 3. The ECO2 hardware traps on these instructions.

**Fix:** Added to `sdkconfig.defaults`:
```ini
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
CONFIG_ESP32P4_REV_MIN_100=y
```

**Status:** ✅ Permanently resolved. See ADR-003 for details.

### 2.2 C6 Companion Wi-Fi Crash Loop (Day 3 — Resolved)

**Symptom:** C6 firmware entered infinite reboot loop with
`ESP_ERR_WIFI_NOT_STARTED` when calling `esp_wifi_set_channel()`.

**Root Cause:** The ESP-IDF Wi-Fi driver state machine requires
`esp_wifi_start()` to complete **before** any channel configuration.
Our initialization called `esp_wifi_set_channel()` before `esp_wifi_start()`.

**Fix:** Reordered initialization:
```c
esp_wifi_start();            // Must be first
esp_wifi_set_channel(ch, 0); // Now safe
esp_now_init();              // Then ESP-NOW
```

**Status:** ✅ Permanently resolved.

### 2.3 UART Data Collision — P4 vs C6 on Shared Traces (Day 4 — Partially Resolved)

**Symptom:** `esptool` reported `Serial data stream stopped: Possible serial
noise or corruption` when flashing the C6 via `/dev/ttyACM5`. The P4's UART
driver was simultaneously driving GPIO14/GPIO15, injecting noise into the
CH344Q hub's data lines.

**Root Cause:** The P4 `ipc_transport` component initializes UART1 on GPIO14/15
during `app_main()`. Since these GPIO lines are physically connected to the C6's
UART0 via the SDIO D0/D1 traces, the P4's transmissions collide with `esptool`'s
flasher protocol bytes.

**Initial Workaround (Successful):**
1. Compile P4 firmware with `CONFIG_MODE_FLASH_COMPANION=y`. This causes the P4
   to enter an infinite `while(1) { vTaskDelay(portMAX_DELAY); }` loop **before**
   initializing `ipc_transport`, releasing the GPIO14/15 lines.
2. Physically jumper C6 IO9 (BOOT pin on H7 header) to GND.
3. Press the board's physical RESET button. Both chips restart. The P4 halts
   before touching the UART. The C6, with IO9 grounded, enters ROM Download Mode.
4. Flash the C6 at 115200 baud with `--before no_reset --no-stub`:
   ```bash
   python -m esptool --chip esp32c6 -p /dev/ttyACM5 -b 115200 \
     --before no_reset --no-stub --after hard_reset write_flash \
     --flash_mode dio --flash_freq 80m --flash_size 2MB \
     0x0 build/bootloader/bootloader.bin \
     0x8000 build/partition_table/partition-table.bin \
     0x10000 build/edge-companion-c6.bin
   ```

**Why `--no-stub` is mandatory:** The default `esptool` behavior uploads a
"flasher stub" (a small program) to the C6's RAM and executes it. On the C6's
USB-Serial/JTAG peripheral (which is managed by the chip's internal USB CDC
stack, not an external UART-to-USB bridge), this stub execution causes a
momentary USB re-enumeration. The CH344Q hub cannot handle this gracefully,
resulting in `StopIteration` / `The chip stopped responding` errors.

**Why `--before no_reset` is mandatory:** The standard `default_reset` sequence
sends DTR/RTS pulses to toggle EN and BOOT. Through the CH344Q hub, these
signals either do not reach the C6's strapping pins at all, or they cause the
USB CDC stack to disconnect, triggering `OSError: [Errno 71] Protocol error`
(a `TIOCMSET` ioctl failure because the file descriptor is no longer valid).

### 2.4 C6 Flashing — Permanent Failure (Day 6 — UNRESOLVED)

**Symptom:** After successfully flashing the C6 once (Day 4) using the workaround
described in §2.3, all subsequent flash attempts failed with identical errors:

```
esptool.util.FatalError: Serial data stream stopped: Possible serial noise or corruption.
A fatal error occurred: The chip stopped responding.
```

**Attempted mitigations (all failed):**

| Attempt | Parameters | Result |
|---|---|---|
| Standard flash | `-b 115200 --before default_reset` | `OSError: [Errno 71] Protocol error` |
| No-reset + stub | `-b 115200 --before no_reset` | `Serial data stream stopped` at `get_security_info` |
| No-reset + no-stub | `-b 115200 --before no_reset --no-stub` | `Serial data stream stopped` at `get_security_info` |
| Ultra-low baud | `-b 19200 --before no_reset --no-stub` | `Serial data stream stopped` at `get_security_info` |
| Baud sweep script | `9600, 19200, 38400, 74880` | All failed identically |

**Analysis:**

The failure pattern is consistent: `esptool` establishes a SLIP connection
(the `Connecting....` line succeeds, indicating the C6 ROM bootloader is
responding to sync bytes), but immediately fails when it sends the first
real command (`get_security_info`). This indicates that:

1. The C6 **is** in Download Mode (ROM bootloader is alive and responding to
   sync pulses).
2. The C6 ROM bootloader **receives** the `get_security_info` command.
3. The C6 ROM bootloader **begins to respond**, but the response is either
   corrupted or truncated by the time it reaches `esptool` through the CH344Q
   hub.

**Probable root causes (non-exclusive):**

- **CH344Q Hub Bandwidth Contention:** The quad-port hub may not reliably handle
  bidirectional traffic on the C6's USB-Serial/JTAG interface when other ports
  (P4 JTAG, P4 UART0) are simultaneously enumerated, even if idle.
- **USB CDC Re-enumeration Race:** The C6's internal USB CDC peripheral may
  momentarily reset its USB state during certain ROM bootloader operations,
  causing the hub to drop buffered bytes.
- **Power Rail Instability:** When the C6 accesses its internal flash (triggered
  by `get_security_info` reading eFuse data), a transient current spike may cause
  a brownout on the shared 3.3V rail, collapsing the USB CDC stack.

**The single successful flash (Day 4)** likely succeeded due to a fortunate
combination of timing (the C6 had been freshly power-cycled with a clean ROM
state, no prior USB CDC negotiation had occurred, and the hub's internal buffers
were empty).

---

## 3. Board Verdict

| Criterion | Verdict | Notes |
|---|---|---|
| P4 Host development | ✅ Excellent | 400MHz dual-core, PoE Ethernet, MicroSD, ECDSA_DS |
| P4 ↔ C6 UART IPC | ⚠️ Functional with caveats | Works at 460800 bps through 51KΩ series resistors. Requires careful GPIO initialization sequencing. |
| C6 flashing via USB | ❌ Unreliable | Shared CH344Q hub makes `esptool` flash-via-USB-CDC non-deterministic. Requires manual BOOT jumper + frozen P4. |
| C6 flashing via H7 header | ⚠️ Requires P4 frozen | External UART adapter works, but P4 must not be driving GPIO14/15. |
| Host-Driven OTA (esp-serial-flasher) | ❌ Not feasible | P4 cannot reliably toggle C6 EN/BOOT via GPIO54/GPIO6. |
| Production deployment | ❌ Not recommended | C6 firmware cannot be reliably updated in the field. |

### Recommendation

This board is suitable for **P4-only development** (Ethernet, MicroSD, crypto).
For dual-chip P4+C6 architectures requiring reliable C6 firmware updates, use
the **ESP32-P4X-Function-EV-Board** (Espressif official), which provides:
- Dedicated USB ports per SoC (no shared hub)
- Proper EN/BOOT strapping control from host
- Official Espressif support and errata documentation

---

## 4. Migration Path

This project (`edge-telemetry-gateway`) has been frozen on its `main` branch
as a reference implementation. Active development continues in two repositories:

| Repository | Target Hardware | Status |
|---|---|---|
| `edge-s3-gateway` | ESP32-S3 + W5500 (SPI Ethernet) | **Active** — interim gateway for field deployment |
| `edge-telemetry-gateway` | ESP32-P4X-Function-EV-Board | **Paused** — awaiting hardware delivery (ETA: 3 months) |

When the ESP32-P4X-Function-EV-Board arrives, the P4 firmware from this
repository can be reused with minimal changes (replace `ip101` PHY driver
with the board's native Ethernet configuration, remove `CONFIG_MODE_FLASH_COMPANION`
workaround).

## 5. References

- Waveshare Product Page: https://www.waveshare.com/esp32-p4-wifi6-poe-eth.htm
- ESP32-P4 Technical Reference Manual (Espressif)
- ESP32-C6-MINI-1 Module Datasheet (Espressif)
- ESP-IDF esptool documentation: https://docs.espressif.com/projects/esptool/
- ADR-001: Bypass ESP-Hosted
- ADR-002: UART IPC over SDIO traces
- ADR-003: ESP32-P4 ECO2 rev 1.3 silicon workarounds
