# Focus

- **Next session starts with (Opus):** branch `main`, focus **#31 agent install** — finish the hardware proofs.
  1. On the WROOM-32 (ttyUSB0): replace `hello31` with a new image of the same name; a keep=false run-once;
     a bad signature (§9 test 4); a power cut mid-fetch (§9 test 3). Expected console lines: #31 trial comment.
  2. Pushes (obs/build, obs/apps) are refused until rete#443 is merged and the gateway porta redeployed:
     the agent's 6-element row is the target, do not add a 5-element path (rete-29, 2026-11-17).
  3. README: WiFi/agent builds now need `prj_agent.conf` in `EXTRA_CONF_FILE`; `west.yml` fork pin once pushed.
- **Boards:** WROOM-32 `build-i31` (WiFi + agent, `hello31` installed OTA), logger `i31-wroom-d-20261117.log`.
  C6 47a4 `build-i31-c6-agent` (Thread + agent, `hello31` installed OTA), logger `i31-c6-20261117.log`.
  Dongle 2dbf still `build-m25-nrf` on the old fork, logger 1730461. Battery nRF52840: `rtt_drain.py` 1405701.
  Local confs (git-ignored) now carry `CONFIG_TOIT_AGENT_PUBKEY` and, for Thread, `CONFIG_TOIT_AGENT_PORTA`.
- **Zephyr tree:** `~/zephyr-esp32/zephyr` carries `patches/zephyr-nrf-802154-hfclk-ready-deferred.patch`
  uncommitted; re-apply after any west update (`docs/design/zephyr-port.md` addendum 2026-11-17).
- **Awaiting David:** NOTHING IS PUSHED since `20a235e`: `main` and fork `wip-plan2-fix-a` (`7fb2838e`) wait
  for the host tool's flash/run, to go out together (David 2026-11-17). Remote fork `wip-plan2-fix-a` is a
  stale pre-rebase commit: new name or OK to overwrite. The live Thread network key is in public rete
  (`nodus/nsl/docs/design/2026-07-11-rung7-openthread-l3-design.md:61`): rotate? #26, #33 upstream reports.
  XIAO nRF54L15 Sense. Remove worktrees `~/ztoit-wroom32`, `~/toit-wroom32`?
- **Decided:** install stages to `scratch_partition`, verifies, reboots, `main()` installs before the VM
  (design addendum 2026-11-17). Uninstall waits for rete#519. `FlashRegistry::flush()` must really flush on
  Espressif (#33). WiFi credentials come from the settings store (#30). Never halt a running nRF (#23).
- **Open P2:** #31, #30, #23, #18, #8, #12, #4, #24. **P3:** #2, #3, #5, #9, #10, #13–#17, #20–#22, #26–#29, #32, #33.
