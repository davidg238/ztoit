# Focus

- **Next session starts with (Opus):** branch `main`, #47. First: read the dongle soak on 4.5
  (`~/.local/state/ztoit/i47-nrf-2dbf-fd-20261122.log`, logger 305771; FATAL_DUMP build `build-i47-nrf-fd`; 518/518 at
  43 min); if clean,
  close #47 citing the three boards' runs. Then faber #8: prove `faber flash` on the nRF54L15 (David, 2026-11-21).
- **Boards (all Zephyr 4.5):** C6 47a4 (ttyUSB0) `build-i47-c6`, Thread + agent, logger `i47-c6-20261121d.log`.
  WROOM-32 f58c (by-path `…usb-0:1.4.1:1.0-port0`, ztoit's per rete 2026-11-22) `build-i47-wroom`, logger
  `i47-wroom-f58c-20261122.log`. Dongle 2dbf (by-id `…CDC_ACM_serial_backend_695DC675…`, renamed by 4.5). XIAO runs
  `~/zephyr-4.5/build-rung7` (logger 11798). rete has the EzSBC at by-path `1.3`: hands off.
- **Fork:** work is on `wip-plan2-fix-a`, pushed as `zephyr-port` (b3964baa); the local `zephyr-port` branch is stale.
- **Awaiting David:** #37 header resolder; #26/#33 upstream reports on hold. rete keeps `~/zephyr-esp32` until its
  own 4.5 migration (rete's call, 2026-11-22): don't remove it.
- **Decided:** One Zephyr, upstream v4.5.0 (`west.yml`, addenda 2026-11-21/22 in `docs/design/zephyr-port.md`).
  No history scrub (2026-11-19). Thread key in public rete accepted (2026-11-18). nRF: sleep, not deep sleep;
  never halt a running nRF (#23).
- **Open P2:** #47, #37, #23, #18, #8, #12. **P3:** #5, #9–#11, #14–#17, #20–#22, #26, #27, #29, #33, #39, #40, #43–#46, #48, #49.
