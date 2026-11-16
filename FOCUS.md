# Focus

- **Next session starts with (Opus):** branch `main`, focus **#10** — finish Plan 2's final-review fixes (A + I2) on
  fork branch `wip-plan2-fix-a` (`99b45eae`, unverified). First action: check that branch out in `$TOIT_SRC` and rerun
  `tools/fork-sdk.sh` (the host compiler must match the branch's primitives), then build for the C6. Then: C6 child
  table R=0, soak rtt at 100 ms poll, dongle rerun; commit to `zephyr-port`, bump the pin, close the ledger
  `.superpowers/sdd/2026-11-15-nodus-toit-plan2-network/`. Each flash needs David's go.
- **Boards:** C6 runs the normal `bb866c16` build + the heap line with `net_cycle` (attached, cycling), logger
  `net-c6-heapfix-20261115.log`. Dongle 2dbf soaks on `bb866c16` firmware (built before the heap line), logger PID
  782249 → `soak-nrf-2dbf-net-20261115.log`. a6b3 silent (#12), logger 632379. `rete-custos` stays disabled.
- **Awaiting David:** go for each reflash; XIAO nRF54L15 Sense (due week of 11-16).
- **Decided:** #19 was the 512 B mbedTLS heap, fixed by `CONFIG_MBEDTLS_HEAP_SIZE` in `zephyr/prj_thread.conf`
  (`docs/design/2026-11-15-thread-first-attach.md`, Resolution) — no attach-in-`main()` fallback. "Down" = SED poll
  period, never rx-on/off flips (design §7 addendum 2026-11-15). Thread tests must start from erased OT settings
  (`erase_region 0x3b0000 0x30000`). A silent OpenThread failure gets one `CONFIG_ASSERT=y` run before bisecting
  (on the C6 it then stops at #5).
- **Open P2:** #18 (echo drops after attach; rete F11 ND lead), #8, #12, #4. **P3:** #2, #3, #5, #9, #13–#17, #20.
