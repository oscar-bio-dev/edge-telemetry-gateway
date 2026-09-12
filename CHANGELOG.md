# Changelog

All notable changes to this project will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0-rc.1] - 2026-09-12

### Added
- Complete End-to-End telemetry flow (ESP-NOW -> ESP32-C6 -> ESP32-P4 -> GCP Pub/Sub).
- Offline Spooler for resilient SD card storage during network outages.
- mTLS authentication for Cloud Transport.

### Changed
- Re-architected C6 flashing procedure via physical BOOT-strap isolation (ADR-006).

### Security
- Excluded development private keys (`.pem`) from version control.
