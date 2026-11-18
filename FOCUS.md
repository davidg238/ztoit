# Focus

- **Next session starts with (Opus):** branch `main`, focus **#31 agent install** — the last hardware proofs,
  on the WROOM-32 (`build-i31`): a bad signature refused (§9 test 4; a build with another `CONFIG_TOIT_AGENT_PUBKEY`
  does it without touching the gateway), a reset mid-fetch (§9 test 3), then push `obs/apps` after a run-once erase.
- **Boards:** WROOM-32 `build-i31` WiFi + agent, logger 1980180 (`i24-wroom-20261117.log`). C6 47a4
  `build-i31-c6-agent` Thread + agent, logger 1941375 (`i24-c6-20261117.log`). Both run `hello31`, installed
  over the air. Dongle 2dbf `build-m25-nrf` on fork `d6a1d7fe`, logger 1730461: lossy since 20:10 (#27).
  Battery nRF52840 on the J-Link: `rtt_drain.py` 1405701.
- **Zephyr tree:** `~/zephyr-esp32/zephyr` carries `patches/zephyr-nrf-802154-hfclk-ready-deferred.patch`
  uncommitted; re-apply after any west update (`docs/design/zephyr-port.md` addendum 2026-11-17).
- **Awaiting David:** push `main` (the /conclude commits; never pushed unasked). Was dongle 2dbf touched or its
  USB replugged around 20:00 on 2026-11-17 (#27)? The Thread network key in public rete is not the OpenThread
  default (`nodus/nsl/docs/design/2026-07-11-rung7-openthread-l3-design.md:61`): rotate or accept. #26 and #33
  upstream reports. XIAO nRF54L15 Sense.
- **Decided:** install stages, verifies, reboots, `main()` installs (design addenda 2026-11-17). Fork work is
  on `zephyr-port`, pinned in `west.yml`. The host tool stays private; the README is fine as it is (David
  2026-11-17). No IPv6 on the WROOM-32. Uninstall waits for rete#519. Never halt a running nRF (#23).
- **Open P2:** #31, #23, #18, #8, #12, #4. **P3:** #2, #3, #5, #9–#11, #13–#17, #20–#22, #26–#29, #32, #33.
