# ADR 005: MicroSD SDMMC VFS & Degraded Mode

## Context and Problem Statement

The Edge Telemetry Gateway requires persistent storage for two distinct use cases:
1. **Emergency Spooling (Store-and-Forward):** In the event of a network partition or HTTPS mTLS failure with the backend, telemetry data must be buffered offline without risk of data loss.
2. **Edge AI (ESP-DL):** The Gateway will execute local ML models (e.g., anomaly detection). This requires storing Neural Network weights (`.tflite` / `.espdl`) and a rolling window of historical telemetry data to feed the inference engine upon reboot.

Initially, the ESP32-P4's internal Flash memory (using SPIFFS or NVS) was considered. However, the high write cycles caused by continuous telemetry spooling and historical logging would lead to rapid **wear-out**, permanently damaging the SoC's storage. 

## Decision

We will use the **external MicroSD slot** present on the Waveshare ESP32-P4-WIFI6-POE-ETH board as the primary persistent storage medium.

1. **Hardware Interface:** The MicroSD slot will be driven using the **SDMMC peripheral in 4-bit mode** (Slot 0: CLK=43, CMD=44, D0-D3=39-42), powered by the internal LDO (Channel 4). This avoids the SPI bus bottleneck and maximizes throughput for ESP-DL tensor loading.
2. **VFS Architecture:** A FAT filesystem will be mounted at `/sdcard`. The directories are strictly separated:
   - `/sdcard/spool/` for the emergency Store-and-Forward mechanism.
   - `/sdcard/ai/` for ESP-DL models and historical inference contexts.
3. **Graceful Degradation (Degraded Mode):** MicroSD cards are consumable components that can fail, corrupt, or be physically removed. The firmware will tolerate this gracefully:
   - **Boot:** If mounting fails, the system will *not* abort. It will enter `DEGRADED_MODE`.
   - **Runtime:** Continuous I/O errors will logically unmount the card and trigger `DEGRADED_MODE`.
   - **Effects of Degraded Mode:** ESP-DL inference is suspended (returning the Gateway to a pure "passthrough" telemetry router), the Spooler is disabled (falling back exclusively to the RAM FreeRTOS Queue), and a `SystemHealthEvent` is dispatched to the backend to alert operators of the required physical maintenance.

## Status

Accepted.

## Consequences

- **Positive:** Hardware lifespan of the ESP32-P4 is protected from write amplification. ESP-DL has ample, high-speed storage for large model weights.
- **Positive:** High resilience; the system remains functional for core telemetry routing even if the SD card fails.
- **Negative:** Increased state machine complexity (`NORMAL_MODE` vs `DEGRADED_MODE`).
- **Negative:** Requires careful handling of the internal LDO and SDMMC slot initialization to avoid initialization races during boot.
