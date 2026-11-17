# Focus

- **Next session starts with (Opus):** branch `main`, focus **#23 dongle hang** (blocks #10 step 2 on nRF).
  1. Build Plan 2 for the dongle with `CONFIG_LOG` immediate + fault dump (fork `wip-plan2-fix-a`, `tools/fork-sdk.sh`
     first, Thread conf per #22), DFU to 2dbf (David presses RESET), read the CDC log for a fatal. Ruled-out list in #23.
  2. If silent: bisect 445a28ac→08e79a85 on the dongle (thread_up/down, network.toit, `thread_down` arity).
  3. #18 (second): make carrier-sense CCA the C6 default, then split 47a4's residual 0.54 % at the gateway (#18 17:54).
- **Boards (all mine to reflash, memory):** 47a4 (ttyUSB1) Plan 2 + counters + carrier CCA, logger 1089768.
  2dbf hung on the erase-settings debug image, logger 1241315. a6b3 silent (#12), logger 632379.
  DK-base nRF52840 on the J-Link, Plan 2 soak, console over RTT (`JLinkExe savebin`, never halt it).
  c6spare unplugged (its USB port feeds the J-Link). `rete-custos` stays disabled.
- **Awaiting David:** XIAO nRF54L15 Sense. Whether to tell rete its C6 conf has the same -60 dBm ED CCA (#18).
- **Decided:** #18 RX fixed (fork 445a28ac); #18 TX = C6 ED CCA false busy, carrier sense 3× better (#18 17:54).
  #19 = 512 B mbedTLS heap. Plan 2 attaches from erased settings on C6 + DK nRF (#10). Never halt a running nRF to
  diagnose (it froze the clock, #23). Fork rebased branch is `wip-plan2-fix-a-445` on the fork remote.
- **Open P2:** #23, #18, #8, #12, #4. **P3:** #2, #3, #5, #9, #13–#17, #20–#22.
