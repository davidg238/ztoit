# Focus

- **Next session starts with (Opus), overnight 2026-11-16, unattended:** branch `main`. David's brief: fixes,
  stability, soak — whatever is productive, to about 40 % context, then pause until morning with a status on the
  issues. **C6 reflashes and settings erases are pre-approved for this night only**; the dongles need a RESET press,
  so read their logs but do not flash them. Suggested order:
  1. Check the C6 since 23:38 on 11-15: `grep -a "thread:\|net_cycle\|FATAL" ~/.local/state/ztoit/net-c6-heapfix-20261115.log | tail`
     and the 2dbf soak log; post what they show on #18 / #8 / #9.
  2. **#18** (echo drops ~30 s after attach; rete F11 ND lead) on the C6: re-measure first, then root cause.
  3. Plan 2's final-review fixes (A + I2, #10) on fork branch `wip-plan2-fix-a` (`99b45eae`, unverified): check it
     out in `$TOIT_SRC`, rerun `tools/fork-sdk.sh`, build, verify on the C6 (child table R=0, rtt at 100 ms poll);
     commit to `zephyr-port`, bump the pin. The dongle rerun waits for David.
  4. Leave the C6 on a long soak with a logger before pausing.
- **Boards:** C6 runs the normal `bb866c16` build + the heap line with `net_cycle`, logger
  `net-c6-heapfix-20261115.log`. Dongle 2dbf soaks on `bb866c16` firmware (built before the heap line), logger PID
  782249 → `soak-nrf-2dbf-net-20261115.log`. a6b3 silent (#12), logger 632379. `rete-custos` stays disabled.
- **Awaiting David:** dongle RESET presses; XIAO nRF54L15 Sense (due week of 11-16).
- **Decided:** #19 was the 512 B mbedTLS heap (`docs/design/2026-11-15-thread-first-attach.md`, Resolution). "Down" =
  SED poll period, never rx-on/off flips (design §7 addendum 2026-11-15). Thread tests start from erased OT settings.
  A silent OpenThread failure gets one `CONFIG_ASSERT=y` run before bisecting (on the C6 it then stops at #5).
- **Open P2:** #18, #8, #12, #4. **P3:** #2, #3, #5, #9, #13–#17, #20.
