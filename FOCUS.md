# Focus

- **Next session starts with (Opus):** branch `main`; #52 is closed, so the next P2 is David's pick (proposed: #54,
  the remote console, since f58c now has no serial log). First: re-read #54 against `agent.c`'s `push_obs`.
  f58c is on wall power (boot 70 since 2026-11-27 10:45); its record is `porta log` on gw, not a serial log.
- **Boards:** `~/workspace/board-alloc.md` (David's). ztoit: dongles 2dbf and a6b3 (a6b3's logger is gone, its
  log ends 2026-11-23; #12), C6 47a4 (`build-up26-c6`); it builds f58c's firmware (owner "gw").
- **Fork:** `wip-plan2-fix-a` at 6cd05d94, pushed as `zephyr-port`. Rerun `tools/fork-sdk.sh` after any fork
  commit. Worktree `~/workspaceToit/toit-v2.0.0-picks` holds the host test build (`ninja -C build/host check_slow`).
- **Awaiting David:** next focus issue; is `obs/apps/once` = `erased` after a reboot expected; restart a6b3's logger?; is #11's "raw gpio/i2c/spi"
  line obsolete now that IO comes from devicetree; #37 header resolder; #26/#33 upstream reports on hold. rete keeps
  `~/zephyr-esp32`: don't remove it. David's uncommitted changes in `~/workspaceToit/vindriktning` are untouched.
- **Decided:** ztoit maintains its fork; upstream fixes come by `cherry-pick -x`; IO from Zephyr devicetree, no
  drivers in Toit; `M(zephyr)` stays last (docs/design/2026-11-26-toit-upstream-divergence.md). An observation is
  a bare int (nodus-toit design, addendum 2026-11-25). Board priority: nRF, C6, WROOM-32. Remote console is #54,
  not #52. One Zephyr, v4.5.0. No history scrub. nRF: sleep, not deep sleep; never halt a running nRF (#23).
- **Open:** P2 #54, #37, #23, #18, #8, #12, #4. P3 #2, #3, #5, #9–#11, #14–#17, #20–#22, #26, #27, #29,
  #33, #39, #40, #43–#46, #48–#50, #53, #55–#60.
