# Focus

- **Next session starts with (Opus):** branch `main`. **main is 3 commits ahead of origin, not pushed**
  (no go to push main): `de14962` pins fork `445a28ac`. Focus #18's remaining TX loss:
  1. #18 TX: the C6 overnight soak on 445a28ac lost 107/7178 UDP requests (1.5 %, all hours) that never
     reached the gateway; dongle 2dbf lost 0 after boot (#18 comment 11-16 10:25). First rule out the fix:
     soak the C6 on bb866c16 for 1 h and compare. Then root cause (rete F11 ND lead).
  2. Plan 2 fixes (A + I2, #10): rebase fork `wip-plan2-fix-a` (`99b45eae`) onto `445a28ac`, then as before.
- **Boards:** C6 runs `build-soak` (soak.toit, fork 445a28ac). Dongle 2dbf soaks on `bb866c16` (pre-fix),
  logger 782249. a6b3 silent (#12), logger 632379. `rete-custos` stays disabled.
- **Awaiting David:** push of main; dongle RESET to retest #9 on 445a28ac; XIAO nRF54L15 Sense.
- **Decided:** #18 RX = consume-before-allocate in UDP `receive`, fixed in `445a28ac` (#18 comment 11-16).
  #19 = 512 B mbedTLS heap. "Down" = SED poll period. Thread tests start from erased OT settings.
- **Open P2:** #18, #8, #12, #4. **P3:** #2, #3, #5, #9, #13–#17, #20.
