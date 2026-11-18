# Focus

- **Next session starts with (Opus):** branch `main`, focus **#31 agent install** — finish the hardware proofs.
  1. On the WROOM-32: a bad signature (§9 test 4); a power cut mid-fetch
     (§9 test 3); push obs/apps after a run-once erase. Replace, pushes, run-once are proven (#31).
  2. Rerun `tools/fork-sdk.sh` after any fork commit, or the host tool refuses on the SDK identity (#24).
- **Boards:** WROOM-32 (now ttyUSB2; use the by-id path) `build-i31` (WiFi + agent, `hello31` installed OTA), logger `i24-wroom-20261117.log`.
  C6 47a4 `build-i31-c6-agent` at `f5c2acb` (Thread + agent, `hello31` installed OTA), logger `i24-c6-20261117.log`.
  Dongle 2dbf still `build-m25-nrf` on the old fork, logger 1730461. Battery nRF52840: `rtt_drain.py` 1405701.
  Local confs (git-ignored) now carry `CONFIG_TOIT_AGENT_PUBKEY` and, for Thread, `CONFIG_TOIT_AGENT_PORTA`.
- **Zephyr tree:** `~/zephyr-esp32/zephyr` carries `patches/zephyr-nrf-802154-hfclk-ready-deferred.patch`
  uncommitted; re-apply after any west update (`docs/design/zephyr-port.md` addendum 2026-11-17).
- **Awaiting David:** the Thread network key in public rete is NOT the OpenThread default
  (`nodus/nsl/docs/design/2026-07-11-rung7-openthread-l3-design.md:61`): rotate? #24 to close. #26, #33
  upstream reports. XIAO nRF54L15 Sense. Remove worktrees `~/ztoit-wroom32`, `~/toit-wroom32`, and the
  remote fork branches `wip-plan2-fix-a` (stale) and `wip-plan2-wifi`?
- **Decided:** pushed 2026-11-17: fork `zephyr-port` = `7fb2838e`, pinned in `west.yml`, `main` on origin.
  install stages to `scratch_partition`, verifies, reboots, `main()` installs before the VM
  (design addendum 2026-11-17). Uninstall waits for rete#519. `FlashRegistry::flush()` must really flush on
  Espressif (#33). WiFi credentials come from the settings store (#30). Never halt a running nRF (#23).
- **Open P2:** #31, #30, #23, #18, #8, #12, #4, #24. **P3:** #2, #3, #5, #9, #10, #13–#17, #20–#22, #26–#29, #32, #33.
