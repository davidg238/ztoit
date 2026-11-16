# Focus

- **Next session starts with (Opus):** branch `main`, focus **#18 TX loss**.
  1. Compare the C6 soaks: `grep -a "uptime" ~/.local/state/ztoit/soak-c6-prefix-20261116.log | tail -1`
     (pre-fix udp_zephyr.cc, `build-soak-prefix`, from 10:34 on 11-16) against 445a28ac's 107/7178 UDP
     failures (#18 comment 11-16 10:25). Same rate → the fix is innocent; root-cause the TX loss (requests
     never reach the gateway; rete F11 ND lead). Then reflash `build-soak` (445a28ac).
  2. Plan 2 fixes (A + I2, #10): rebase fork `wip-plan2-fix-a` (`99b45eae`) onto `445a28ac`, then as before.
- **Boards:** C6 on `build-soak-prefix`, logger 1027714. Dongle 2dbf on `bb866c16`, logger 782249
  (0 UDP losses after boot in 16 h). a6b3 silent (#12), logger 632379. `rete-custos` stays disabled.
- **Awaiting David:** dongle RESET to retest #9 on 445a28ac; XIAO nRF54L15 Sense.
- **Decided:** #18 RX = consume-before-allocate in UDP `receive`, fixed in fork `445a28ac` (#18 comment
  11-16 00:3x). #19 = 512 B mbedTLS heap. "Down" = SED poll period. Thread tests start from erased OT settings.
- **Open P2:** #18, #8, #12, #4. **P3:** #2, #3, #5, #9, #13–#17, #20, #21.
