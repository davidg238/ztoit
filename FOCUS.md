# Focus

- **Next session starts with (Opus):** branch `main`, focus **#25 merge `wroom32-wifi`** (WROOM-32 WiFi + C agent).
  1. Merge ztoit `wroom32-wifi` (`86a1e21`, worktree `~/ztoit-wroom32`) into main; conflict in `zephyr/CMakeLists.txt`.
  2. Fork: put `4ba762ae` (worktree `~/toit-wroom32`) on `wip-plan2-fix-a` (same base 08e79a85), read the
     `Thread::current()` change first, `tools/fork-sdk.sh`, then soak C6 47a4 + dongle 2dbf (RESET press) on it.
  3. Tell rete-29 the result (it holds both branches still until then). Re-run `hello.toit` for #3.
- **Boards:** 2dbf Plan 2 + Zephyr patch + FATAL_DUMP, logger 1359252 (`hfclkfix-nrf-2dbf-20261116.log`).
  Battery nRF52840 on the J-Link: `wake_soak.toit`, console via `~/.local/state/ztoit/dk-rtt-debug/rtt_drain.py`
  (PID 1405701). 47a4 (ttyUSB1) unchanged since 2026-11-16. WROOM-32 on ttyUSB0 is rete-29's.
- **Zephyr tree:** `~/zephyr-esp32/zephyr` carries `patches/zephyr-nrf-802154-hfclk-ready-deferred.patch`
  uncommitted; re-apply after any west update (`docs/design/zephyr-port.md` addendum 2026-11-17).
- **Awaiting David:** push `main` (ahead of origin, never pushed unasked). #26 go to report upstream. Where the
  Zephyr patch lives long-term. XIAO nRF54L15 Sense. Tell rete its C6 conf has the -60 dBm ED CCA (#18)?
- **Decided:** #23 = radio-driver panic with the HF clock already on, fixed by the patch (16.5 h dongle soak, #23).
  A fatal on the dongle is silent over CDC: use `-DFATAL_DUMP=ON`. #18 RX fixed; TX = C6 ED CCA (#18 17:54).
  Merge wroom32-wifi in its own issue, soak before done (#25). Never halt a running nRF (#23).
- **Open P2:** #25, #23, #18, #8, #12, #4, #24. **P3:** #2, #3, #5, #9, #13–#17, #20–#22, #26, #27.
