#!/bin/sh
# Flash a ztoit build to the nRF52840 dongle over its nRF5 DFU bootloader.
#   tools/nrf-flash.sh [build-dir]   (default build-nrf)
# Press the dongle's RESET button first (red LED pulses): its Open bootloader
# enters DFU only from the reset pin (GPREGRET 0xB1 did not work, 2026-11-14).
set -e
B="${1:-build-nrf}"
nrfutil nrf5sdk-tools pkg generate --hw-version 52 --sd-req=0x00 \
  --application "$B/zephyr/zephyr.hex" --application-version 1 "$B/ztoit.zip" >/dev/null
lsusb | grep -q 1915:521f || { echo "dongle not in DFU: press its RESET button"; exit 1; }
sleep 1
nrfutil nrf5sdk-tools dfu usb-serial -pkg "$B/ztoit.zip" -p /dev/ttyACM0
