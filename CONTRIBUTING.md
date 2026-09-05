# Contributing to Edge Telemetry Gateway

## Commit Messages

Follow [Conventional Commits](https://www.conventionalcommits.org/):

- **All commit messages MUST be written in English.**

```
feat: add JWT ES256 signing with ECDSA_DS HW accelerator
fix: resolve CRC16 endianness on little-endian RISC-V
refactor: extract COBS codec into shared_components
chore: update pre-commit hooks to clang-format 18
```

## Code Style

- Run `pre-commit run --all-files` before pushing.
- Use `ESP_LOG*` with `static const char *TAG` — no `printf()`.
- Fixed-width types (`uint32_t`, `int8_t`) in critical paths.

## Pull Requests

- **Strict Rulesets:** Branch protection has been upgraded to Rulesets.
- **Approvals:** Minimum 1 approval required (2 for security/boot/power changes).
- **Required Checks:** `format`, `lint`, `build`, `size`, `unit-tests`, and `security` MUST pass.
- **CODEOWNERS:** Changes to `.github/`, `docs/adr/`, `partitions*`, `boot`, security, or power management require explicit specialized owner review.

## Lessons Learned from CI (Retrospective)

To maintain a green pipeline, all contributors MUST adhere to the following rules discovered during the Bring-Up phase:

1. **`clang-format` is Unforgiving:**
   - **Rule:** You MUST run `pre-commit run --all-files` locally before creating a commit. The GitHub Action will immediately fail the build if there is a single formatting violation.
2. **Missing Files (`.gitignore` vs CMake):**
   - **Rule:** If a binary asset (like `dev_private_key.pem`) is required by CMake's `target_add_binary_data` but is excluded by `.gitignore`, the CI clone will fail to configure. Always provide a fallback (e.g. `*.dummy`) and copy it via `execute_process(cp)` in CMake.
3. **Linker Undefined References (Scope):**
   - **Rule:** In ESP-IDF, if `Component B` consumes binary data (`_binary_*_start`), the `target_add_binary_data` command MUST reside in `Component B`'s `CMakeLists.txt`, NOT in `main/CMakeLists.txt`. Otherwise, the linker will fail due to dependency directionality.
4. **Strict `PRIV_REQUIRES` Dependencies:**
   - **Rule:** `#include <header.h>` is not enough. If your component uses types from `esp_netif` or `telemetry_buffer`, you MUST declare them in the `PRIV_REQUIRES` array of your component's `CMakeLists.txt`.
5. **Zero-Tolerance Warnings (`-Werror`):**
   - **Rule:** CI treats all warnings as errors. Unused variables in `#ifndef` blocks or mock paths MUST be cast to void (e.g., `(void)var;`). Do not assume older ESP-IDF struct members (like `voltage_stable_delay_us`) exist in v5.4 without checking the official API.
6. **Kconfig Targets (`set-target`):**
   - **Rule:** If you delete the `sdkconfig` file to regenerate it, you MUST explicitly run `idf.py set-target esp32p4` before building. Otherwise, CMake defaults to the `esp32` (Xtensa) target, which will aggressively corrupt your RISC-V build and cause linker failures due to missing peripherals.
7. **Silicon Revision Workarounds (v1.3 vs v3.1):**
   - **Rule:** ESP-IDF v6.1 officially defaults to ESP32-P4 v3.1. If you are using ECO1/ECO2 (v1.3) Engineering Samples, you MUST ensure `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` is in your `sdkconfig.defaults`, or the compiler will emit illegal RISC-V extensions, causing the bootloader to panic with an `Illegal instruction` instantly on boot.

> **Note:** For full governance policies, please refer to the Workspace Global Policy (`AGENTS.md`) and the Layer 2 GitHub Standard.
