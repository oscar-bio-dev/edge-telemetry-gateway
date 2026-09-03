/*
 * SPDX-FileCopyrightText: 2026 EcoTech Sensing
 * SPDX-License-Identifier: Apache-2.0
 *
 * Telemetry Decoder — Nanopb static decode of EnvironmentalData.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Decoded telemetry sample with metadata from IPC frame. */
typedef struct {
    uint8_t  src_mac[6];
    int8_t   rssi;
    uint16_t seq_num;
    uint32_t timestamp;
    float    temperature;
    float    humidity;
    float    iaq;
    uint32_t iaq_accuracy;
    uint32_t co2_ppm;
    float    pm1_0;
    float    pm2_5;
    float    pm10_0;
    uint32_t battery_mv;
    uint32_t sleep_cycles;
} decoded_telemetry_t;

/**
 * @brief Decode a Protobuf-encoded EnvironmentalData payload.
 *
 * @param[in]  pb_data   Raw Protobuf bytes.
 * @param[in]  pb_len    Length of Protobuf data.
 * @param[out] out       Decoded telemetry structure.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on decode failure.
 */
esp_err_t telemetry_decode(const uint8_t *pb_data, size_t pb_len,
                           decoded_telemetry_t *out);

#ifdef __cplusplus
}
#endif
