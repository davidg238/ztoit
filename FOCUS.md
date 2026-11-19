# Focus

- **Next session starts with (Opus):** branch `main`, #41 (ktau rung 2: ztoit on Zephyr 4.5 and the XIAO).
  Rung 1 is done (ktau `docs/journal/01-board-on-zephyr-4.5.md`). First: write rung 2's plan with
  superpowers:writing-plans from `~/workspaceToit/ktau/docs/design/2026-11-19-ktau-design.md`, into ktau's
  `docs/superpowers/plans/`. Fallback: #37 after the resolder, rerun scratchpad `lines.toit`/`probe3.toit`
  via `jag run -d 192.168.0.244` (UDP to 192.168.0.175, fresh port each run).
- **Boards:** XIAO nRF54L15 console `…XIAO_nrf54_CMSIS-DAP_E96922FA-if02`, runs hello_world. Camera WROOM-32
  (by-path `…usb-0:1.3:1.0-port0`) runs Jaguar at 192.168.0.244. C6 47a4 runs `build-i38-c6` (logger 2298951).
  Dongle 2dbf logger 1730461. J-Link nRF `rtt_drain.py` 1405701.
- **Awaiting David:** #37 header resolder (then VCC 5 V if still `ff`); a GitHub remote for ktau;
  #26/#33 upstream reports on hold.
- **Decided:** Thread key in public rete accepted (2026-11-18). Refused install pushed `#refused` (#38). Zephyr
  4.5 beside 4.2 (`docs/design/zephyr-port.md` 2026-11-19). ktau design approved. nRF: sleep, not deep sleep.
  Never halt a running nRF (#23).
- **Open P2:** #41, #37, #23, #18, #8, #12, #4. **P3:** #2, #3, #5, #9–#11, #13–#17, #20–#22, #26–#29, #33, #39, #40, #42.
