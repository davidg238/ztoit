# Focus

- **Next session starts with:** branch `main`. Execute nodus-toit **Plan 1**
  (`docs/superpowers/plans/2026-11-15-nodus-toit-plan1-containers.md`, approved, run natively, refs #10). First:
  Task 1, the fork's `system/extensions/zephyr/boot.toit`. Reflashes (C6, dongles) each need David's go.
- **Boards:** C6 (…40db) runs `io.toit` on `/dev/ttyUSB0`. Dongle 2dbf (instrumented soak, s37 build) is
  **silent since 11:51 on 11-15** (#12, P2), left as is. a6b3 runs the old soak (`soak-nrf-a6b3-usb-20261115.log`).
  Loggers: PIDs 632378 (2dbf), 632379 (a6b3). `rete-custos` stays disabled for the loan (`CLAUDE.md`).
- **Awaiting David:** XIAO nRF54L15 Sense (shipping from CA, due week of 11-16).
- **Decided:** `zephyr` primitive module + fork compiler (`docs/design/zephyr-port.md`). Containers stay stock
  Toit; jag/Artemis wire compatibility is not a goal. nodus-toit design `docs/design/2026-11-15-nodus-toit-design.md`:
  C agent + Toit supervisor, CoAP to porta (rete#505), dev tool **faber**, flash console, power
  `on`/`sleep`/`deep-sleep` separate from the radio, sleepy-end-device default. Roadmap #11 revised: step 2 =
  nodus-toit; raw gpio/i2c/spi only as needed. Florian: no rebase (#11). zpen (private) later.
- **#9:** the 5-min summary (`process-stats --gc`) loses the next UDP echo (proved with a 37-round summary);
  next test: summary without `--gc`.
- **Found:** #2, #3, #5, #10, #13: P3. #8, #12: P2.
