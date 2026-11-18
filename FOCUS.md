# Focus

- **Next session starts with (Opus):** branch `main`, focus **#31 agent install** (design step 4, §8 Install).
  1. Bump the `west.yml` fork pin to `c01c01dc` (cc-rete's #30 change; left for us) and `tools/fork-sdk.sh`.
  2. Read `zephyr/src/agent/` and the boot rule in `zephyr/src/main.cc` that erases runtime-installed containers.
  3. Build the install slice on the WROOM-32 (ttyUSB0, WiFi; credentials are in its settings store, #30).
     rete-29 wrote the agent (rete#521) and answers agent questions; the host side already writes `gw/apps/<name>`.
- **Boards:** 47a4 (ttyUSB1) `build-m25-c6`, 2dbf `build-m25-nrf` (FATAL_DUMP), both fork `d6a1d7fe`, loggers
  1728655 / 1730461 still running (`merge25-*-20261117.log`). WROOM-32 runs cc-rete's `build-w30`; its local
  conf porta address was fixed to .176 after that build, so rebuild before testing the agent.
  Battery nRF52840 on the J-Link: `wake_soak.toit`, `rtt_drain.py` PID 1405701.
- **Zephyr tree:** `~/zephyr-esp32/zephyr` carries `patches/zephyr-nrf-802154-hfclk-ready-deferred.patch`
  uncommitted; re-apply after any west update (`docs/design/zephyr-port.md` addendum 2026-11-17).
- **Awaiting David:** push `main` (ahead of origin; held 2026-11-17 until #30 landed — it has, ask again) and
  fork `wip-plan2-fix-a` (remote is the stale pre-rebase commit: needs a new name or his OK to overwrite).
  #26 go to report upstream. Where the Zephyr patch lives long-term. XIAO nRF54L15 Sense.
  Tell rete its C6 conf has the -60 dBm ED CCA (#18)? Remove worktrees `~/ztoit-wroom32`, `~/toit-wroom32`?
- **Decided:** #25 closed: merge soak clean at 4.5 h (#25). WiFi credentials come from the settings store,
  written by the host tool at first flash (#30). #23 fixed by the Zephyr patch; a dongle fatal is silent over
  CDC, use `-DFATAL_DUMP=ON`. #18 RX fixed; TX = C6 ED CCA. Never halt a running nRF (#23).
- **Open P2:** #31, #30, #23, #18, #8, #12, #4, #24. **P3:** #2, #3, #5, #9, #10, #13–#17, #20–#22, #26–#29.
