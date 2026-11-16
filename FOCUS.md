# Focus

- **Next session starts with (Fable):** branch `main`, focus **#19** (C6: first Thread attach fails once the VM is
  up). Read `docs/design/2026-11-15-thread-first-attach.md` (every run, dead ends, best guess, next tests). First
  action: judge the record, then either test 1 there (attach in `main()` after only `FlashRegistry::set_up()`)
  or ship the fallback (attach in `main()` before the VM). Each C6 flash needs David's go.
- **Then:** finish Plan 2's final-review fixes (A + I2) on fork branch `wip-plan2-fix-a` (`99b45eae`, unverified):
  C6 child table R=0, soak rtt at 100 ms poll, dongle rerun; commit to `zephyr-port`, bump the pin, close the
  ledger `.superpowers/sdd/2026-11-15-nodus-toit-plan2-network/`.
- **Boards:** C6 runs a debug image (net_cycle with a 10 s start delay, attach counters; its first attach failed),
  logger `net-c6-late-20261115.log`. Dongle 2dbf soaks on `bb866c16` firmware, logger PID 782249 →
  `soak-nrf-2dbf-net-20261115.log`. a6b3 silent (#12), logger 632379. `rete-custos` stays disabled.
- **Awaiting David:** go for each reflash; XIAO nRF54L15 Sense (due week of 11-16).
- **Decided:** "down" = SED poll period, never rx-on/off flips (design §7 addendum 2026-11-15). The full TLV
  dataset does not fix #19 (record, run 7) — don't retry it. Thread tests must start from erased OT settings
  (`erase_region 0x3b0000 0x30000`): a stored child hides first-attach bugs.
- **Open P2:** #18 (echo drops after attach; rete F11 ND lead), #19, #8, #12, #4. **P3:** #2, #3, #5, #9, #13–#17.
