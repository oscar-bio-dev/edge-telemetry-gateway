#!/bin/bash
source /home/ecotech/.esp/esp-idf/export.sh
python -m esptool --chip esp32c6 -p /dev/ttyACM5 -b 115200 --before no_reset --after hard_reset --no-stub write_flash --flash_mode dio --flash_freq 80m --flash_size 2MB 0x0 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0x10000 build/edge-companion-c6.bin
