# Focus

- **Next session starts with (Opus):** branch `main`. #51 done on hardware (XIAO Thread + agent + faber install, 76f457a);
  pick the next focus from the P2 list with David.
- **Boards (labelled by MAC tail, David 2026-11-22):** ztoit: C6 47a4 (by-path `4`, bare) `build-i47-c6`; C6 6e60 (`1.3`, BMP180); WROOM-32 b37c
  (USB hub, replacing f58c); WROOM-32 f58c → Vindriktning on wall power (#52, P1); dongle 2dbf (by-path `1.4.3`) `build-i47-nrf-fd`. XIAO cd2e (Sense,
  probe E96922FA) → ktau; XIAO 0b41 (no Sense, probe 917C153B) → faber (agent f4ce36a9de2713cd); both run `build-i51-xiao`. Dongle a6b3
  (`1.4.4`, by-id `…CDC_ACM_serial_backend_C1A3FA1CE2137BDB`, #12) → ztoit, `build-i47-nrf-fd` since 21:47, logger 621784. Out: MagTag. rete has no msi board.
- **Fork:** work is on `wip-plan2-fix-a`, pushed as `zephyr-port` (b3964baa); the local `zephyr-port` branch is stale.
- **Awaiting David:** #37 header resolder; #26/#33 upstream reports on hold. rete keeps `~/zephyr-esp32` until its
  own 4.5 migration (rete's call, 2026-11-22): don't remove it.
- **Decided:** Board priority: nRF (52840, 54L15) first, C6 second, WROOM-32 third (David, 2026-11-22).
  One Zephyr, upstream v4.5.0 (`west.yml`, addenda 2026-11-21/22 in `docs/design/zephyr-port.md`).
  No history scrub (2026-11-19). Thread key in public rete accepted (2026-11-18). nRF: sleep, not deep sleep;
  never halt a running nRF (#23). nsl on Toit: research only (`docs/design/2026-11-22-nsl-on-toit-research.md`, rete#530).
- **Open P1:** #52. **P2:** #51 (XIAO Thread; overnight soak not run), #37, #23, #18, #8, #12. **P3:** #5, #9–#11, #14–#17, #20–#22, #26, #27, #29, #33, #39, #40, #43–#46, #48–#50.
