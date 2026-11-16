# Focus

- **Next session starts with (Opus):** branch `main`. **main is 1–2 commits ahead of origin, not pushed**
  (no go to push main): `de14962` pins fork `445a28ac`. Focus #18's remaining TX loss:
  1. Read the C6 soak: `grep -a "FAILED\|uptime" ~/.local/state/ztoit/soak-c6-18fix-20261116.log | tail`
     (fix image, started 00:27 on 11-16, logger PID 942484). Post on #18.
  2. #18 TX: after a slow (~1 s) re-attach, requests never reach the gateway (rounds 1, 3). Reproduce by
     resetting until `thread: up` is >1 s, then root cause (rete F11 ND lead, attach state).
  3. Plan 2 fixes (A + I2, #10): rebase fork `wip-plan2-fix-a` (`99b45eae`) onto `445a28ac`, then as before.
- **Boards:** C6 runs `build-soak` (soak.toit, fork 445a28ac). Dongle 2dbf soaks on `bb866c16` (pre-fix),
  logger 782249. a6b3 silent (#12), logger 632379. `rete-custos` stays disabled.
- **Awaiting David:** push of main; dongle RESET to retest #9 on 445a28ac; XIAO nRF54L15 Sense.
- **Decided:** #18 RX = consume-before-allocate in UDP `receive`, fixed in `445a28ac` (#18 comment 11-16).
  #19 = 512 B mbedTLS heap. "Down" = SED poll period. Thread tests start from erased OT settings.
- **Open P2:** #18, #8, #12, #4. **P3:** #2, #3, #5, #9, #13–#17, #20.
