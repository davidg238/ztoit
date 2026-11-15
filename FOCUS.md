# Focus

- **Deadline (David, 2026-11-14):** 4 h total for #7 I/O + soak review (#8), then David announces on the Toit forum.
- **Next session starts with:** branch `main`. First: soak review — #8 (wall-wart dongle stalled at ~55 min).
  `grep -a "soak: round\|FAILED\|Booting" ~/.local/state/ztoit/soak-nrf2-20261114.log | tail` (USB dongle …2dbf,
  logger PID 242163), and for …a6b3/…40db: `ssh gw85224-01 journalctl -u udp-echo6 --since -1h | grep -c <addr>`
  (12/min each). Post on #8/#4. Then #7: ask David for the C6 pins (switch, BMP180 SDA/SCL), write the overlay.
- **Boards:** C6 (…40db) runs `soak.toit`; flashing it for #7 ends that. WeAct RP2350B on msi USB (STIC; UF2 in
  BOOTSEL). Dongle reflash: msi USB, RESET, `tools/nrf-flash.sh`. gw85224-01: `tcp-echo6` + rete's `udp-echo6`.
  `rete-custos` stays disabled for the loan (`CLAUDE.md`). The second dongle was rete's: rebuild rete's image to return it.
- **Awaiting David:** C6 pin wiring for #7.
- **Decided:** ztoit public, Apache-2.0; peripherals from Zephyr drivers; three SoCs (C6, nRF52840, RP2350B
  RAM registry) — `docs/design/zephyr-port.md` addenda. Test programs ≤8 tasks (#3). DNS skipped for now.
- **Found:** #2, #3, #5: P3. #8: P2.
