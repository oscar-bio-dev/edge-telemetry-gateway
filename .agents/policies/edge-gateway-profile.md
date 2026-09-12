# Edge Telemetry Gateway — Perfil de Repositorio (Capa 3)

> **Anexo normativo específico** del proyecto `edge-telemetry-gateway`.
> Este documento complementa la Capa 1 (AGENTS.md) y la Capa 2 (github-governance.md).
> Terminología normativa: **MUST** (obligatorio), **SHOULD** (recomendado), **MAY** (opcional).

## §1. Declaración de Hardware (Bill of Materials Normativo)

| Rol | Componente | Detalle |
|-----|-----------|---------|
| **MCU Host** | ESP32-P4 | RISC-V dual-core HP (400 MHz) + LP (32 MHz), ECO2, rev 1.3 |
| **MCU Companion** | ESP32-C6 | RISC-V single-core, WiFi 6 + BLE 5 + IEEE 802.15.4 |
| **Placa de Desarrollo** | Waveshare ESP32-P4-WIFI6-POE-ETH | v1.3, integra P4 + C6 en carrier board |
| **PHY Ethernet** | IP101GRI | RMII, 100 Mbps, PHY Addr = 1 |
| **Storage** | MicroSD (slot onboard) | SDMMC 4-bit, Slot 0 (GPIO 39-44 en ESP32-P4) |
| **PMU** | LDOs internos del P4 | Canal 4 → 3.3V para MicroSD |
| **IPC** | UART (GPIO14/15) | COBS/CRC16, 460800 baud, P4 ↔ C6 |
| **Conectividad** | Ethernet (PoE) + WiFi 6 (vía C6) | Gateway segregado: criptografía en P4, radio en C6 |

## §2. Erratas de Silicio y Workarounds Obligatorios

### 2.1 ESP32-P4 ECO2 Rev 1.3 — "Illegal Instruction" Panic

**Síntoma:** Guru Meditation Error `Illegal instruction` en `call_start_cpu0` del bootloader al arrancar.

**Causa raíz:** El silicio ECO2 rev 1.3 no es compatible con instrucciones RISC-V que el toolchain genera para rev ≥ 3. El IDF aborta si detecta una revisión inferior sin el flag explícito.

**Workaround obligatorio en `sdkconfig.defaults`:**
```
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
CONFIG_ESP32P4_REV_MIN_100=y
```

> ⚠️ **REGLA INMUTABLE:** Estos flags MUST estar presentes en `sdkconfig.defaults` de TODO firmware que corra en esta placa. Eliminarlos provoca un brick inmediato del bootloader.

**Referencia:** ADR-003, validado con `esp-pilot-mcp/catalog_list_socs`.

### 2.2 PHY Ethernet IP101 — Reset Pin No Conectado

En la placa Waveshare, el pin de reset del PHY IP101GRI **no está cableado** a ningún GPIO del P4. Por lo tanto:
```
CONFIG_ETH_PHY_RST_GPIO=-1
```
El driver MUST omitir la secuencia de reset por GPIO y confiar en el power-on-reset del PHY.

## §3. Compatibilidad Cross-Version de Mbed TLS

El código criptográfico (JWT, TLS) MUST compilar contra **Mbed TLS 2.x** (ESP-IDF ≤ 5.1) **Y** **Mbed TLS 3.x** (ESP-IDF ≥ 5.4).

### APIs Divergentes Conocidas

| Función | Mbed TLS 2.x | Mbed TLS 3.x |
|---------|-------------|-------------|
| `mbedtls_pk_parse_key()` | 5 argumentos | 7 argumentos (añade `f_rng`, `p_rng`) |
| `mbedtls_pk_sign()` | 7 argumentos (sin `sig_size`) | 9 argumentos (añade `sig_size`, `f_rng`, `p_rng`) |

### Patrón Obligatorio
```c
#include "mbedtls/build_info.h"
#include "mbedtls/version.h"

#if MBEDTLS_VERSION_NUMBER >= 0x03000000
    // Inicializar CTR-DRBG + Entropy como RNG
    int ret = mbedtls_pk_parse_key(&pk, key, len, NULL, 0,
                                   mbedtls_ctr_drbg_random, &ctr_drbg);
#else
    int ret = mbedtls_pk_parse_key(&pk, key, len, NULL, 0);
#endif
```

> ⚠️ **REGLA:** Todo nuevo código en `cloud_transport` o `jwt_generator` que toque APIs de `mbedtls/pk.h` MUST usar este patrón de guards. Violarlo romperá el CI (que usa `espressif/idf:v5.4` con Mbed TLS 3.x).

**Referencia:** ADR-004, incidente CI del 2026-09-06.

## §4. Topología Dual-Firmware

```
┌─────────────────────────────────┐      UART IPC       ┌──────────────────────┐
│        HOST (ESP32-P4)          │◄────────────────────►│  COMPANION (ESP32-C6)│
│                                 │  COBS/CRC16 460800  │                      │
│  • Orquestación (main)          │  GPIO15(TX)→RX_C6   │  • WiFi 6 Radio      │
│  • Ethernet (IP101 RMII)        │  GPIO14(RX)←TX_C6   │  • BLE 5.0           │
│  • Cloud Transport (HTTPS/JWT)  │                     │  • 802.15.4 (Thread) │
│  • MicroSD Storage (FATFS)      │                     │  • Sensor Aggregation│
│  • Telemetry Decoder (Nanopb)   │                     │  • ESP-NOW (futuro)  │
│  • Edge AI / ESP-DL (futuro)    │                     │                      │
│  • Companion OTA (futuro)       │                     │                      │
└─────────────────────────────────┘                     └──────────────────────┘
```

### Reglas de Partición de Responsabilidades
- **Criptografía pesada** (TLS, JWT, Protobuf encoding) MUST ejecutarse en el Host P4.
- **Radio** (WiFi, BLE, 802.15.4) MUST ejecutarse en el Companion C6.
- **IPC** entre P4 y C6 MUST usar tramas COBS/CRC16 sobre UART, no SPI/SDIO.
- **OTA del Companion**: Se realizará desde el Host vía `esp-serial-flasher` (futuro).
- **Inmutabilidad del Companion**: El código en la carpeta `companion/` se considera **estable y congelado**. Debido a la complejidad manual del flasheo, no debe modificarse bajo ninguna circunstancia para refactorizaciones menores o mejoras triviales. "Si funciona, no se toca".

## §5. Pinout Map — Waveshare ESP32-P4-WIFI6-POE-ETH

> Mapa de pines críticos validados en laboratorio. Solo se listan los pines con asignación funcional confirmada.

### 5.1 Ethernet (RMII — IP101GRI)

| GPIO | Función | Notas |
|------|---------|-------|
| 31 | `ETH_MDC` | SMI Clock |
| 52 | `ETH_MDIO` | SMI Data |
| — | `ETH_PHY_RST` | **No conectado** (`-1`) |
| _Internos_ | RMII TXD/RXD/CRS_DV/TX_EN/CLK | Manejados por el driver EMAC, no requieren config explícita |

**PHY Address:** `1` (`CONFIG_ETH_PHY_ADDR=1`)

### 5.2 MicroSD (SDMMC Slot 0)

| GPIO | Función | Notas |
|------|---------|-------|
| 39 | `SD_CLK` | Reloj SDMMC |
| 40 | `SD_CMD` | Comando |
| 41 | `SD_D0` | Dato 0 |
| 42 | `SD_D1` | Dato 1 |
| 43 | `SD_D2` | Dato 2 |
| 44 | `SD_D3` | Dato 3 |

**PMU:** LDO Canal 4 → 3.3V (`CONFIG_SDCARD_LDO_CHANNEL=4`)
**Pull-ups:** Internos habilitados (`SDMMC_SLOT_FLAG_INTERNAL_PULLUP`)

### 5.3 UART IPC (P4 ↔ C6)

| GPIO | Función | Notas |
|------|---------|-------|
| 15 | `UART_TX` (P4 → C6) | `CONFIG_IPC_UART_TX_GPIO=15` |
| 14 | `UART_RX` (C6 → P4) | `CONFIG_IPC_UART_RX_GPIO=14` |

**Baudrate:** 460800 (`CONFIG_IPC_UART_BAUD_RATE=460800`)
**Protocolo:** COBS framing + CRC-16/CCITT

### 5.4 USB-JTAG (Debug)

| Pin | Función | Notas |
|-----|---------|-------|
| USB-C | USB-JTAG integrado | Para debug con OpenOCD (`type: espidf` en `launch.json`) |

## §6. Checks de CI Específicos del Gateway

### Build Matrix Obligatorio
El pipeline CI MUST compilar ambos firmwares:

| Firmware | Target | Path | Imagen Docker |
|----------|--------|------|---------------|
| Host | `esp32p4` | `.` (raíz) | `espressif/idf:v5.4` |
| Companion | `esp32c6` | `./companion` | `espressif/idf:v5.4` |

### Exclusiones de Terceros
Los siguientes directorios MUST excluirse de `clang-format` y hooks de estilo (tanto en `.pre-commit-config.yaml` como en el script del workflow):
- `components/cjson/` — Librería cJSON upstream (Dave Gamble)
- `components/ip101/` — Driver IP101GRI portado de ESP-IDF examples
- `shared_components/nanopb/` — Generador Nanopb upstream
- `shared_components/proto/*.pb.*` — Archivos auto-generados por Nanopb

### Size Gate
- Binario del Host MUST caber en partición factory (1 MB = `0x100000` bytes).
- El CI SHOULD reportar el tamaño en el Step Summary de GitHub.

## §7. Riesgos Conocidos y Mitigaciones

| # | Riesgo | Severidad | Mitigación | Estado |
|---|--------|-----------|-----------|--------|
| R1 | Silicio ECO2 rev 1.3 incompatible con instrucciones rev≥3 | 🔴 Crítico | `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` | ✅ Mitigado |
| R2 | Mbed TLS 2.x/3.x API break en CI | 🟠 Alto | Guards con `MBEDTLS_VERSION_NUMBER` | ✅ Mitigado |
| R3 | Nanopb `pb_callback_t` causa panics con strings | 🟠 Alto | `.options` con `max_size` para todos los strings | ✅ Mitigado |
| R4 | PHY IP101 sin pin de reset → no hay recovery de Latch-up | 🟡 Medio | Confiar en power-on-reset; watchdog como fallback | ⚠️ Aceptado |
| R5 | Un solo desarrollador → bus factor = 1 | 🟡 Medio | Documentación exhaustiva, ADRs, gobernanza | 🔄 En progreso |
| R6 | OTA del companion aún no implementado | 🟡 Medio | `esp-serial-flasher` planificado para Fase 4 | ⏳ Pendiente |
| R7 | Edge AI (ESP-DL) aún no integrado | 🟡 Medio | Planificado para Fase 3 | ⏳ Pendiente |

## §8. Roadmap de Hardening de Seguridad

> Fuente: MCP server `espressif-engineering/security/troubleshoot-security` — workflow de 5 fases con checklist de Secure Boot v2, Flash Encryption, y NVS Encryption para ESP32-P4.

### Timeline Propuesto

| Fase | Feature | Prioridad | Dependencia |
|------|---------|-----------|-------------|
| Post-Fase 3 | **Secure Boot v2** (ECDSA, eFuse key) | Alta | Estabilidad de firmware |
| Post-Fase 3 | **Flash Encryption** (Development → Release) | Alta | Secure Boot primero |
| Post-Fase 4 | **NVS Encryption** (HMAC-based) | Media | Flash Encryption habilitado |
| Post-Fase 4 | **OTA Seguro** (firma de imágenes) | Alta | Secure Boot + Transport TLS |

### Checklist Pre-Hardening (del MCP)
- [ ] Confirmar chip capability matrix para ESP32-P4 (eFuse layout, key slots)
- [ ] Elegir tool path: Flash Download Tool vs `esptool` vs software-first
- [ ] Documentar estrategia de key management (per-device vs shared)
- [ ] Crear ADR dedicado para decisiones de seguridad
