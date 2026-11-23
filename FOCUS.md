# Focus

- **Next session starts with (Opus):** branch `main`. #51 done on hardware (XIAO Thread + agent + faber install, 76f457a);
  pick the next focus from the P2 list with David.
- **Boards:** `~/workspace/board-alloc.md`. ztoit has 2dbf, a6b3, 47a4; f58c goes to wall power for #52.
- **Fork:** work is on `wip-plan2-fix-a`, pushed as `zephyr-port` (b3964baa); the local `zephyr-port` branch is stale.
- **Awaiting David:** #37 header resolder; #26/#33 upstream reports on hold. rete keeps `~/zephyr-esp32` until its
  own 4.5 migration (rete's call, 2026-11-22): don't remove it.
- **Decided:** One Zephyr, upstream v4.5.0 (`west.yml`, addenda 2026-11-21/22 in `docs/design/zephyr-port.md`).
  No history scrub (2026-11-19). Thread key in public rete accepted (2026-11-18). nRF: sleep, not deep sleep;
  never halt a running nRF (#23). nsl on Toit: research only (`docs/design/2026-11-22-nsl-on-toit-research.md`, rete#530).
- **Open P1:** #52. **P2:** #51 (XIAO Thread; overnight soak not run), #37, #23, #18, #8, #12. **P3:** #5, #9–#11, #14–#17, #20–#22, #26, #27, #29, #33, #39, #40, #43–#46, #48–#50.
