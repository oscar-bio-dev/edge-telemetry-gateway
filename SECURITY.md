# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| 0.x     | ✅        |

## Reporting a Vulnerability

Report security vulnerabilities via GitHub private advisories or email
security@oscar-bio-dev. We will respond within 72 hours.

## Cryptographic Architecture (ESP-IDF v6.1)

This project has been fully migrated to **MbedTLS 3.x** and relies exclusively on the **PSA Crypto API**. 

- **JWT Signing:** The Edge Telemetry Gateway uses `psa_sign_message` (PSA_ALG_ECDSA_ANY) instead of the deprecated `mbedtls_pk_sign` function. Entropy injection is handled securely and transparently by the PSA subsystem; manual `f_rng` callbacks MUST NOT be used in this codebase.
- **Hardware Acceleration:** The ESP32-P4's `ECDSA_DS` peripheral is enabled. In production, JWTs are signed using eFuse-burned keys. For development and CI, a fallback key (`dev_private_key.pem.dummy`) is provided. This file is safely committed to source control and is excluded from `.gitignore`.

## SLA

- Critical: Patch within 7 days
- High: Patch within 30 days
- Medium/Low: Next scheduled release
