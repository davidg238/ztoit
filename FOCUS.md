# Focus

- **Next session starts with (Opus):** branch `main`. If David has plugged the XIAO nRF54L15 into msi, it's
  ktau rung 1 Task 2 (`~/workspaceToit/ktau/docs/superpowers/plans/2026-11-19-rung1-board-on-zephyr-4.5.md`,
  ledger in `ktau/.superpowers/sdd/`). Diff `/dev/serial/by-id`, `west flash -d ~/zephyr-4.5/build-blinky`,
  David confirms the LED, then hello_world and journal 01. Rung 2 is #41. Otherwise #37: David resoldered
  the camera header. Rerun the scratchpad `lines.toit` and `probe3.toit` via `jag run -d 192.168.0.244`
  (UDP output to 192.168.0.175, on a fresh port each run).
- **Boards:**
  - Camera WROOM-32 (by-path `…usb-0:1.3:1.0-port0`) runs Jaguar "camera" at 192.168.0.244.
  - C6 47a4 runs `build-i38-c6`; logger 2298951 (`i38-c6-20261118.log`).
  - Dongle 2dbf logger 1730461. J-Link nRF `rtt_drain.py` 1405701.
- **Awaiting David:**
  - Rung 1 LED check (XIAO).
  - #37 header resolder, then VCC at 5 V if it's still `ff`.
  - A GitHub remote for ktau.
  - Was dongle 2dbf touched (#27)? Unknown.
  - #26/#33 upstream reports: on hold.
- **Decided:**
  - Thread network key in public rete: accepted (David 2026-11-18).
  - A refused install is pushed `#refused` (#38).
  - Zephyr 4.5 lives beside 4.2 (`docs/design/zephyr-port.md` addendum 2026-11-19).
  - ktau design: approved (`ktau/docs/design/2026-11-19-ktau-design.md`).
  - On nRF: sleep, not deep sleep.
  - Never halt a running nRF (#23).
- **Open P2:** #41, #37, #23, #18, #8, #12, #4. **P3:** #2, #3, #5, #9–#11, #13–#17, #20–#22, #26–#29, #33, #39, #40, #42.
