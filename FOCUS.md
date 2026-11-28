# Focus

- **Next session starts with (Fable designs, Opus codes):** branch `main`; #62 stage 3, `uart`. `gpio` and `i2c`
  are done and proven on C6 boards. First: read `lib/uart.toit` and `ztoit.Uart`'s RX path in the fork's
  `src/resources/ztoit.cc`; keep `ztoit.Uart` working (the Vindriktning app on f58c uses it). Then `spi`.
- **Boards:** `~/workspace/board-alloc.md` (David's). 47a4 boots `tests/board/i2c_bmp180.toit`, which fails
  there (no BMP180); jumper GPIO2 to GPIO11 (the silk labels sit one pin off). 6e60 is lent by rete and runs the
  same build. f58c (owner "gw") is on msi USB with `build-i62-vin`: console, `ztoit` module, random fix.
- **Fork:** `wip-plan2-fix-a` at 6b2a9a93, pushed as `zephyr-port`. Rerun `tools/fork-sdk.sh` after any fork
  commit. `west` needs `~/zephyr-4.5/env.sh` and then ztoit's `env.sh`, in the same shell command.
- **Tests:** `tools/test-qemu.py -d build-i62-qemu`: 383 of 436 (`tests/qemu/results-20261127e.txt`), ztoit's
  own as `ztoit/<name>`. Board tests are boot containers: `-DCONTAINERS=…/tests/board/x.toit`, no hyphen in the name.
- **Awaiting David:** go for `uart`; #54: close it, or first add a way to read the UART drop count (the app
  never closes its UART); is `obs/apps/once` = `erased` expected; restart a6b3's logger?; #37 resolder.
  ktau must move to `import ztoit` and the current fork. Untouched: David's changes in `~/workspaceToit/vindriktning`.
- **Decided:** standard libraries on devicetree-routed pins, bus pins as numbers, transfers on the VM thread,
  `toit,platform` + `ztoit,buses` nodes (zephyr-port.md, addendum 2026-11-27). Module and library are `ztoit`;
  ztoit keeps its fork, upstream by `cherry-pick -x` (2026-11-26-toit-upstream-divergence.md). Remote console:
  nodus-toit design, addendum 2026-11-27. Board priority nRF, C6, WROOM-32. Never halt a running nRF (#23).
- **Open:** P2 #62, #61, #54, #37, #23, #18, #12, #8, #4. P3 #2, #3, #5, #9–#11, #14–#17, #20–#22, #26,
  #27, #29, #33, #39, #40, #43–#46, #48–#50, #53, #55–#60, #63–#67, #69–#71.
