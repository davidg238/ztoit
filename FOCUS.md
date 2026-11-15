# Focus

- **Next session starts with:** branch `main`. Roadmap #11 step 1, robustness. First: did …a6b3 survive the night
  on msi USB? `grep -a "uptime\|FAILED\|Booting" ~/.local/state/ztoit/soak-nrf-a6b3-usb-20261114.log | tail`
  (logger PID 373733); …2dbf: `soak-nrf2-20261114.log` (PID 242163). Post on #8: stable → wall wart. Then #9
  (UDP fails on round N×60+1, after each stats report), then #3.
- **Boards:** C6 (…40db) runs `io.toit` (BMP180, BOOT, `do0` LED, RGB) on `/dev/ttyUSB0`, off the soak. Both dongles
  on msi USB. RP2350B on msi. `rete-custos` stays disabled for the loan (`CLAUDE.md`).
- **Awaiting David:** XIAO nRF54L15 Sense (~week of 11-16).
- **Decided:** `zephyr` primitive module, appended last, + fork compiler (`docs/design/zephyr-port.md` addendum).
  Roadmap order #11 (David 2026-11-15). Upstream Zephyr port (toitlang/toit #3322–#3333) is slow; Florian: no
  rebase, his agents would redo it — ztoit stays independent, the design doc is the handoff (#11 comment).
  OTA install #10. zpen (private) is later.
- **Found:** #2, #3, #5, #9, #10: P3. #8: P2.
