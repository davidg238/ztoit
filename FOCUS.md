# Focus

- **Next session starts with (Opus):** branch `main`, #41 (ktau rung 8: battery — sleep current, current per
  instruction, the 2.0 V check). Rung 7 is done (ktau `docs/journal/07-tailnet-receiver.md`, #41 comment 2026-11-21).
  First: write rung 8's plan with superpowers:writing-plans from the ktau design. Fallback: #37 after the resolder.
- **Fork:** work is on `wip-plan2-fix-a`, pushed as `zephyr-port` (b3964baa); the local `zephyr-port` branch is stale.
- **Boards:** XIAO nRF54L15 console `…XIAO_nrf54_CMSIS-DAP_E96922FA-if02` runs `~/zephyr-4.5/build-rung7`
  (bonded to the Pixel relay; ktau's receiver on msi); logger 11798, log `ktau-rung2-xiao-20261119.log`. C6 47a4 runs the old `build-i38-c6`
  (logger 49855); the build dir was rebuilt (new image ids) but not flashed. Dongle 2dbf logger 49856. J-Link is rete's. Camera WROOM-32 (by-path `…usb-0:1.3:1.0-port0`) runs Jaguar at 192.168.0.244.
- **Awaiting David:** #37 header resolder (then VCC 5 V if still `ff`); #26/#33 upstream reports on hold.
- **Decided:** No history scrub (2026-11-19). Thread key in public rete accepted (2026-11-18). Refused install
  pushed `#refused` (#38). Zephyr 4.5 beside 4.2; Mbed TLS 4 via header shim. I2C, sensor keys, `dmic`, one LC3
  encoder (`docs/design/zephyr-port.md`). UDP over BLE: `udp_ble.cc` beside `udp_zephyr.cc`, bond in ZMS, 7.5–15 ms
  interval always while on USB, on-demand is #46 (addendum 2026-11-21). nRF: sleep, not deep sleep. Never halt a
  running nRF (#23).
- **Open P2:** #41, #37, #23, #18, #8, #12. **P3:** #5, #9–#11, #13–#17, #20–#22, #26–#29, #33, #39, #40, #42–#46.
