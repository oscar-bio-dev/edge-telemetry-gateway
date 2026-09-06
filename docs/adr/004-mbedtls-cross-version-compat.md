# ADR-004: Mbed TLS Cross-Version Compatibility Strategy

- **Status:** Accepted
- **Date:** 2026-09-06
- **Context:** CI uses `espressif/idf:v5.4` Docker image (Mbed TLS 3.x), while local dev may use ESP-IDF v5.1 or v6.1 (Mbed TLS 2.x).

## Problem

The ESP-IDF CI pipeline failed with compilation errors in `jwt_generator.c`:

```
error: too few arguments to function 'mbedtls_pk_parse_key'
error: too few arguments to function 'mbedtls_pk_sign'
```

Mbed TLS 3.x introduced breaking API changes to cryptographic signing functions, adding mandatory RNG parameters (`f_rng`, `p_rng`) and a `sig_size` buffer length parameter. Code written for Mbed TLS 2.x will not compile against 3.x, and vice versa.

### Affected Functions

| Function | Mbed TLS 2.x Signature | Mbed TLS 3.x Signature |
|----------|------------------------|------------------------|
| `mbedtls_pk_parse_key` | `(ctx, key, keylen, pwd, pwdlen)` — 5 args | `(ctx, key, keylen, pwd, pwdlen, f_rng, p_rng)` — 7 args |
| `mbedtls_pk_sign` | `(ctx, md_alg, hash, hash_len, sig, sig_len, f_rng, p_rng)` — 8 args | `(ctx, md_alg, hash, hash_len, sig, sig_size, sig_len, f_rng, p_rng)` — 9 args |

## Decision

Use compile-time version detection via the `MBEDTLS_VERSION_NUMBER` macro to branch between API signatures:

```c
#include "mbedtls/build_info.h"
#include "mbedtls/version.h"

#if MBEDTLS_VERSION_NUMBER >= 0x03000000
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#endif

// ...

#if MBEDTLS_VERSION_NUMBER >= 0x03000000
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);

    int ret = mbedtls_pk_parse_key(&pk, key, len, NULL, 0,
                                   mbedtls_ctr_drbg_random, &ctr_drbg);
#else
    int ret = mbedtls_pk_parse_key(&pk, key, len, NULL, 0);
#endif
```

### Rules

1. All code in `cloud_transport` and `jwt_generator` that calls `mbedtls/pk.h` functions MUST use `#if MBEDTLS_VERSION_NUMBER` guards.
2. For Mbed TLS 3.x, a CTR-DRBG + Entropy context MUST be initialized and passed as the RNG source.
3. The CTR-DRBG and Entropy contexts MUST be properly freed in the cleanup path.
4. This pattern is documented in the Layer 3 profile (`edge-gateway-profile.md §3`).

## Consequences

- **Positive:** Firmware compiles cleanly on both ESP-IDF v5.1 (Mbed TLS 2.x) and v5.4+ (Mbed TLS 3.x).
- **Positive:** CI pipeline passes without requiring pinning to a specific Mbed TLS version.
- **Positive:** RNG injection in 3.x path provides cryptographically stronger key parsing and signing.
- **Negative:** Slight code complexity increase due to `#if` guards. Acceptable trade-off.
- **Negative:** Two code paths to maintain; any future Mbed TLS API changes will require updating both branches.

## Validation

- Local build: `idf.py build` passes with 0 errors (ESP-IDF v6.1, Mbed TLS 2.x).
- CI build: GitHub Actions with `espressif/idf:v5.4` (Mbed TLS 3.x) passes after fix.
- Format check: `pre-commit run --all-files` passes.
