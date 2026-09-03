#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 EcoTech Sensing
# SPDX-License-Identifier: Apache-2.0
#
# Flash the ESP32-C6 Companion firmware via the H7 debug header.
#
# Usage:
#   ./scripts/flash_companion.sh [PORT]
#
# Default port: /dev/ttyUSB1 (typically H7 on Waveshare board)

set -euo pipefail

PORT="${1:-/dev/ttyUSB1}"
COMPANION_DIR="$(dirname "$0")/../companion"

echo "=== Flashing Edge Companion C6 ==="
echo "Port: ${PORT}"
echo "Project: ${COMPANION_DIR}"

cd "${COMPANION_DIR}"

# Ensure target is set
idf.py set-target esp32c6

# Build
idf.py build

# Flash via UART (H7 header)
idf.py -p "${PORT}" flash

echo "=== Companion C6 flashed successfully ==="
echo "To monitor: idf.py -p ${PORT} monitor"
