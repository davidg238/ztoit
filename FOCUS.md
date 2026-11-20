# Focus

- **Next session starts with (Opus):** branch `main`, #41 (ktau rung 6: ztoit `net.udp` over rete's `udp_ble.c`
  through the Pixel relay; echo to a tailnet host). Rung 5 is done (ktau `docs/journal/05-lc3-encode.md`). First:
  write rung 6's plan with superpowers:writing-plans from the ktau design; read `rete/protocol/RELAY-BLE.md` and
  `udp_ble.c`, and find which rete sample proves the relay as the C first check. Fallback: #37 after the resolder.
- **Fork:** work is on `wip-plan2-fix-a` (pushed as `zephyr-port`, 3dbabffb); the local `zephyr-port` branch is stale.
- **Boards:** XIAO nRF54L15 console `…XIAO_nrf54_CMSIS-DAP_E96922FA-if02` runs the rung 5 push-to-talk demo
  (`~/zephyr-4.5/build-rung5`); logger 2649520, log `ktau-rung2-xiao-20261119.log`. Camera WROOM-32 (by-path
  `…usb-0:1.3:1.0-port0`) runs Jaguar at 192.168.0.244. C6 47a4 runs `build-i38-c6` (logger 2298951). Dongle 2dbf
  logger 1730461. J-Link `rtt_drain.py` 1405701.
- **Awaiting David:** #37 header resolder (then VCC 5 V if still `ff`); #26/#33 upstream reports on hold.
- **Decided:** No history scrub (2026-11-19). Thread key in public rete accepted (2026-11-18). Refused install
  pushed `#refused` (#38). Zephyr 4.5 beside 4.2; Mbed TLS 4 via header shim. I2C is one generic primitive; sensor
  interrupts are `gpio-keys`; the microphone is one generic `dmic` group (`docs/design/zephyr-port.md` 2026-11-19).
  One LC3 encoder; a new `Lc3Encoder` takes it over (journal 05). ktau: push to talk on the user button; FPU on for
  the XIAO (ktau design, Decided). nRF: sleep, not deep sleep. Never halt a running nRF (#23).
- **Open P2:** #41, #37, #23, #18, #8, #12, #4. **P3:** #2, #3, #5, #9–#11, #13–#17, #20–#22, #26–#29, #33, #39,
  #40, #42–#45.
