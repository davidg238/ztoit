# Focus

- **Next session starts with (Opus):** branch `main`, #52 step 2: a Toit→agent push primitive whose first user is
  `obs/vin/pm25` (design doc 2026-11-15 §9, the "post obs cells" primitive). First: read how `agent.c` queues
  `obs/apps` pushes, then check porta's accepted value shape for a new `obs/` path. f58c runs `build-i52-vin`
  (logger 688156, `i52-vin-f58c-20261122.log`, by-path `1.4.2`). It moves to wall power once step 2 holds.
- **Boards:** `~/workspace/board-alloc.md` (David's). ztoit: dongles 2dbf and a6b3 (a6b3 on 4.5 soak since
  2026-11-22, #12), C6 47a4; it builds f58c's firmware (owner "gw").
- **Fork:** `wip-plan2-fix-a` at 9a518399, pushed as `zephyr-port`. Rerun `tools/fork-sdk.sh` after any fork commit.
- **Awaiting David:** #37 header resolder; #26/#33 upstream reports on hold. rete keeps `~/zephyr-esp32` until its
  own 4.5 migration: don't remove it. David's uncommitted changes in `~/workspaceToit/vindriktning` (README, mqttsn,
  provision…) are untouched.
- **Decided:** Board priority: nRF first, C6 second, WROOM-32 third (board-alloc.md). #52 is P1; the remote
  console is #54, kept out of #52 (David, 2026-11-22). Readings go to `obs/vin/pm25`. One Zephyr, v4.5.0. No
  history scrub. nRF: sleep, not deep sleep; never halt a running nRF (#23).
- **Open:** P1 #52. P2 #54, #37, #23, #18, #8, #12. P3 #5, #9–#11, #14–#17, #20–#22, #26, #27, #29, #33, #39, #40,
  #43–#46, #48–#50.
