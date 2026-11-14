# Focus

- **Now:** branch `main`, focus #6 (nRF52840 dongle port). Dongle is on msi USB in Open DFU bootloader (`1915:521f`).
- **Tonight:** overnight soak running since 2026-11-14 15:28 on the C6. Read it with
  `grep -a "soak: round\|FAILED" ~/.local/state/ztoit/soak-overnight-20261114.log | tail`, post on #4, then `kill`
  the logger (PID 170400) before flashing the C6.
- **Board state:** the C6 runs `soak.toit`; gw85224-01 runs user unit `tcp-echo6` (:7777). Opening the C6's serial
  port resets it; end captures on `ztoit: exit reason`. Build steps in `README.md` (`west.yml` workspace).
  `rete-custos` stays disabled for the loan (`CLAUDE.md`).
- **Awaiting David:** nothing.
- **Decided:** ztoit is public, Apache-2.0; Toit VM stays LGPL-2.1, libs MIT (`README.md`).
- **Decided:** peripherals come from Zephyr drivers via devicetree, not Toit drivers (`docs/design/zephyr-port.md`).
- **Decided:** port shape in `docs/design/zephyr-port.md` + addenda. Toit source = fork branch `zephyr-port` (SSH).
- **Queued:** #7 (switch + BMP180 via Zephyr APIs), P2. **Found:** #2, #3, #5: P3. DNS skipped for now.
