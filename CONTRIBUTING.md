# Contributing to Edge Telemetry Gateway

## Commit Messages

Follow [Conventional Commits](https://www.conventionalcommits.org/):

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

- Link to the relevant Issue.
- Minimum 1 approval required (2 for security/boot/power changes).
- All CI checks must pass.
