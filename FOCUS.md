# Focus

- **Next session starts with (Sonnet to read, Opus if it failed):** branch `main`, focus **#25 read the merge soak**.
  1. `grep -a` both logs in `~/.local/state/ztoit/`: `merge25-c6-47a4-20261117.log` (started 12:44, logger 1728655)
     and `merge25-nrf-2dbf-20261117.log` (12:50, logger 1730461). Compare echo loss with the pre-merge logs
     (`ab-carrier-c6-47a4-20261116.log`, `hfclkfix-nrf-2dbf-20261116.log`); close #25 if clean overnight.
  2. Then #3 on the C6 (163840 B arena): 100-task `hello.toit` on the page-map fork (#3 2026-11-17).
- **Boards:** 47a4 (ttyUSB1) `build-m25-c6`, 2dbf `build-m25-nrf` (FATAL_DUMP), both on fork `d6a1d7fe`.
  Battery nRF52840 on the J-Link: `wake_soak.toit`, console via `~/.local/state/ztoit/dk-rtt-debug/rtt_drain.py`
  (PID 1405701). WROOM-32 on ttyUSB0 is rete-29's (its `build-wroom32-wifi` image restored 2026-11-17).
- **Zephyr tree:** `~/zephyr-esp32/zephyr` carries `patches/zephyr-nrf-802154-hfclk-ready-deferred.patch`
  uncommitted; re-apply after any west update (`docs/design/zephyr-port.md` addendum 2026-11-17).
- **Awaiting David:** `main` is 2 ahead of origin (`29683dc` README + this) — DO NOT push until #30 lands (David 2026-11-17). #26 go to report upstream. Where the
  Zephyr patch lives long-term. XIAO nRF54L15 Sense. Tell rete its C6 conf has the -60 dBm ED CCA (#18)?
  Remove worktrees `~/ztoit-wroom32`, `~/toit-wroom32` once rete-29 is done with them?
- **Decided:** #23 = radio-driver panic with the HF clock already on, fixed by the patch (16.8 h dongle soak, #23).
  A fatal on the dongle is silent over CDC: use `-DFATAL_DUMP=ON`. #18 RX fixed; TX = C6 ED CCA (#18 17:54).
  wroom32-wifi merged `a60917e`; soak before #25 closes (#25). Never halt a running nRF (#23).
  WiFi credentials move to a run-time store, cc-rete builds it, faber writes them; no shell stopgap (#30).
  Fork is pushed as `wip-plan2-wifi`; remote `wip-plan2-fix-a` is the stale pre-rebase commit, not forced.
- **Open P2:** #25, #30, #23, #18, #8, #12, #4, #24. **P3:** #2, #3, #5, #9, #13–#17, #20–#22, #26–#29.
