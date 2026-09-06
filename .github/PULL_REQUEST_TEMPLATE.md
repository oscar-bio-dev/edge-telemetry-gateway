## Descripción

<!-- Describe brevemente el cambio y su propósito. Vincula el Issue relacionado. -->

Fixes #

## Tipo de Cambio

- [ ] `feat:` Nueva funcionalidad
- [ ] `fix:` Corrección de bug
- [ ] `refactor:` Refactorización sin cambio funcional
- [ ] `docs:` Solo documentación
- [ ] `chore:` Mantenimiento, CI, dependencias
- [ ] `test:` Añadir o modificar tests

## Componentes Afectados

- [ ] `main/` (Orquestación)
- [ ] `components/cloud_transport/` (Transporte cloud)
- [ ] `components/eth_manager/` (Ethernet)
- [ ] `components/ipc_transport/` (UART IPC P4↔C6)
- [ ] `components/storage_manager/` (MicroSD)
- [ ] `components/telemetry_decoder/` (Nanopb)
- [ ] `companion/` (Firmware ESP32-C6)
- [ ] `shared_components/` (Proto, COBS/CRC)
- [ ] `.github/` / CI/CD
- [ ] `docs/` / ADRs
- [ ] `sdkconfig*` / Configuración de build

## Checklist de Validación (DoD)

### Build
- [ ] Compila en CI sin warnings en rutas críticas (`-Werror=all`)
- [ ] Build Host (ESP32-P4) pasa ✅
- [ ] Build Companion (ESP32-C6) pasa ✅
- [ ] `idf.py size` — sin regresión de tamaño significativa

### Calidad
- [ ] `pre-commit run --all-files` pasa localmente
- [ ] Tests unitarios pasan (si aplica)
- [ ] Nanopb: strings usan `max_size` en `.options` (no `pb_callback_t`)

### Hardware (si aplica)
- [ ] Probado en placa física (Waveshare ESP32-P4-WIFI6-POE-ETH)
- [ ] `sdkconfig.defaults` mantiene `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`
- [ ] Monitor serial muestra boot exitoso sin panics

### Documentación
- [ ] `CHANGELOG.md` actualizado
- [ ] ADR creado/actualizado (si es decisión arquitectónica)
- [ ] README refleja estado real del proyecto

## Riesgos Conocidos

<!-- Documenta cualquier riesgo, limitación, o deuda técnica que este PR introduce. -->

- Ninguno / ...

## Screenshots / Logs (si aplica)

<!-- Pega aquí logs del monitor serial, capturas de CI, o evidencia de hardware. -->
