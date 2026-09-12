# AGENTS.md — Normativa Global del Workspace (ESP-IDF)

> **Normativa de cumplimiento obligatorio** para todo agente o desarrollador en este workspace.
> Terminología normativa: **MUST** (obligatorio), **SHOULD** (recomendado), **MAY** (opcional).

## 0) Estructura de Políticas (3 Capas)
Este documento representa la **Capa 1 (Política Global Ejecutiva)**. Todo repositorio en este workspace MUST adherirse además a:
- **Capa 2 (Estándar GitHub):** `policies/github-governance.md` (Rulesets, CI/CD, Supply Chain).
- **Capa 3 (Perfil del Gateway):** `policies/edge-gateway-profile.md` (Hardware BOM, erratas de silicio, pinout, topología dual-firmware, riesgos).

> ⚠️ **INSTRUCCIÓN CRÍTICA PARA EL AGENTE:**
> Antes de modificar código de hardware (GPIO, PMU/LDO, Ethernet, SDMMC), criptografía (mbedTLS, JWT), o la topología dual-firmware (IPC UART P4↔C6), **MUST** leer primero `policies/edge-gateway-profile.md` para consultar erratas de silicio, pinout validado, y patrones obligatorios.

> ⚠️ **CONSULTA MCP OBLIGATORIA:**
> Para cualquier cambio que afecte inicialización de hardware o selección de componentes, el agente MUST consultar proactivamente:
> - `esp-pilot-mcp` → `catalog_list_socs`, `catalog_list_bmgr_boards`, `catalog_get_bmgr_doc`
> - `espressif-engineering` → `ts_skill_get` (channels: `hardware`, `security`, `solution`)
> - `espressif-documentation` → `search_espressif_sources` (para APIs y ejemplos oficiales)

## 2) Flujo de trabajo y trazabilidad
- Todo cambio no trivial MUST estar vinculado a un Issue.
- Commits MUST seguir Conventional Commits (`feat:`, `fix:`, `refactor:`, `chore:`...).
- `CHANGELOG.md` MUST seguir Keep a Changelog + SemVer.
- Puntos de Restauración Seguros (Git): Una vez completado el "scaffolding", es obligatorio inicializar el repositorio y generar un commit inicial.
- README MUST reflejar estado real del proyecto; no se permite “feature drift”.
- Decisiones de arquitectura críticas MUST registrarse en `docs/adr/`.

## 3) Arquitectura de Software e Infraestructura
- `/main` MUST contener solo orquestación y arranque.
- Drivers/HAL/servicios MUST vivir en `/components/<modulo>`.
- Cada componente MUST tener: `include/*.h` (API pública), `*.c` (implementación), `CMakeLists.txt` y tests unitarios.
- APIs entre componentes MUST ser explícitas y desacopladas.
- **Topología Segregada (Gateway / Air-Gap):** Para aplicaciones conectadas a la nube, el nodo sensor MUST utilizar comunicaciones de radio ultracortas de milisegundos (ej. ESP-NOW) hacia un Gateway dedicado. Las pesadas rutinas criptográficas (TLS/JWT/TCP-IP) se delegan a un SoC anclado a la pared (ej. ESP32 + Ethernet WT32-ETH01).
- **Regla de Inmutabilidad del C6 Companion ("Si funciona, no se toca"):** El firmware del ESP32-C6 (directorio `companion/`) actúa como un proxy pasivo. Dado el alto riesgo y fricción manual para flashearlo, su código base está **congelado**. Queda estrictamente prohibido (MUST NOT) modificar, refactorizar o alterar este código a menos que sea para resolver un bug crítico o realizar una mejora arquitectónica mayor previamente aprobada en un ADR.

## 4) Concurrencia, Núcleos y Tiempo Real
- En dual-core, tareas críticas MUST crearse con `xTaskCreatePinnedToCore()`.
- Stack de red SHOULD residir en Core 0 y hardware/sensores en Core 1.
- Bloqueos activos MUST evitarse; usar notificaciones, colas, event groups y `vTaskDelay`.
- Gestión de Memoria Segura: Favorecer variables estáticas locales. Prohibido usar `malloc`/`free` en rutas calientes (hot paths) para evitar fragmentación.

## 5) Resiliencia y Manejo de Errores
- Ningún `esp_err_t` puede ignorarse. Se MUST usar manejo explícito con `ESP_LOGE` + ruta de recuperación.
- Tolerancia a Fallos de Hardware (Sanity Checks): Previo a inicializar buses (I2C/SPI), se MUST validar el estado eléctrico de los pines e inyectar mecanismos de recuperación (ej. 9 pulsos de reloj Bit-Banging para Latch-Up).
- Timeouts/reintentos MUST definirse por componente y documentarse.

## 6) Energía y Deep Sleep
- Estado entre ciclos MUST persistirse con `RTC_DATA_ATTR` (mínimo footprint).
- **Aislamiento de Hardware (Pin Retention):** Durante el Deep Sleep, el dominio de energía principal colapsa. Es obligatorio aislar los dominios RTC (ej. `esp_sleep_pd_config`) y retener el estado lógico usando `gpio_hold_en()` sobre pines que alimenten buses externos (I2C) para prevenir apagones en sensores ópticos o corrientes parásitas.
- Cada módulo crítico SHOULD exponer métricas de consumo/latencia por ciclo.

## 7) Calidad de Código C/C++
- Logs MUST usar `ESP_LOG*` con `static const char *TAG`. Ningún proyecto usará llamadas directas a `printf()` para diagnóstico.
- Tipado fijo MUST usar `<stdint.h>` para rutas críticas.
- Garantía Automática (Pre-Commit): Todo proyecto MUST incluir configuración de `pre-commit` ligada a `clang-format` para garantizar estilo inmaculado.
- Headers nuevos MUST incluir licencia/copyright.
- Regla de warnings en CI: **0 warnings** en rutas críticas.

## 8) Testing y Quality Gates
- Proyecto MUST incluir tests (Unity) para componentes críticos.
- Gate mínimo sugerido: Cobertura unit tests >= 80%, build exitoso, sin regresión de tamaño/heap.
- Pruebas HIL SHOULD cubrir: Recovery de bus, wake/sleep repetido, y fallas de radio.

## 9) Seguridad de Supply Chain (Delegado)
- Las políticas sobre CodeQL, Secret Scanning, Push Protection y Hardening de GitHub Actions (mínimo privilegio, pinning por SHA) MUST regirse por la **Capa 2**: `policies/github-governance.md`.

## 10) CI/CD mínimo obligatorio
- Pipeline MUST ejecutar: Format/lint (`clang-format`, `clang-tidy`), `idf.py build`, `idf.py size`, y tests unitarios.
- Artefactos por PR SHOULD incluir binarios y reporte de tamaño.
- Releases MUST usar tags SemVer y notas de versión.

## 11) Definición de Hecho (DoD)
Un cambio se considera “Done” solo si:
1. Código compila en CI y pasa todos los Linters/Formatters.
2. Tests y checks (Sanity) pasan.
3. Documentación y changelog actualizados.
4. Riesgos documentados en PR.
5. Aprobación requerida obtenida.

## 12) Tooling y Entorno de Desarrollo (VS Code + ESP-IDF)
- **Extensión Oficial MUST ser la única fuente de verdad:** Para compilación, flasheo y monitorización se usarán exclusivamente las herramientas de la Extensión Oficial de ESP-IDF (Status Bar). Queda terminantemente prohibido usar extensiones genéricas (ej. *CMake Tools* de Microsoft) para evitar corrupción del `build/` (ej. falsos `build.ninja` compilados con `gcc` del sistema).
- **Aislamiento de Configuración:** Los proyectos MUST evitar rutas absolutas hardcodeadas (ej. `/home/user/`) en el `.vscode/settings.json`. Se debe delegar la resolución de entornos de Python y SDKs al *ESP-IDF Installation Manager (EIM)* o usar variables dinámicas de la extensión cuando sea seguro.
- **IntelliSense Nativo:** El archivo `c_cpp_properties.json` MUST delegar la resolución de dependencias declarando `"configurationProvider": "espressif.esp-idf-extension"`.
- **Debugging por Hardware:** Para SoCs con USB-JTAG integrado (ej. ESP32-P4/C6), el `launch.json` MUST configurarse usando `"type": "espidf"` apuntando a OpenOCD nativo, permitiendo *step-by-step debug* (F5).
- **Resolución de Fallos (Bring-up / Fallback):** Si la UI de la extensión colapsa por desincronización de Node.js/Python, los desarrolladores MUST validar la operatividad del hardware ejecutando `idf.py build flash monitor` directamente desde la terminal integrada antes de intentar reconfigurar la extensión.
