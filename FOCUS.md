# Focus

- **Next session starts with:** branch `main`, focus #7 (switch + BMP180 on the C6 via Zephyr gpio-keys and sensor
  APIs). First action: ask David which C6 GPIOs the switch and BMP180 SDA/SCL are wired to, then write the overlay.
- **Soak (#4):** nRF52840 dongle on a wall wart since 2026-11-14 16:56, no console. Read it from the gateway:
  `ssh gw85224-01 journalctl -u udp-echo6 --since -10min | grep -c a6b3:ece2:10ef:10d3` (expect 12/min).
- **Boards:** the C6 still runs `soak.toit` (…40db) until flashed for #7; opening its port resets it. Dongle
  reflash: back on msi USB, RESET press, `tools/nrf-flash.sh`. gw85224-01 runs `tcp-echo6` + rete's `udp-echo6`.
  `rete-custos` stays disabled for the loan (`CLAUDE.md`). Thread build: `-DEXTRA_CONF_FILE="prj_thread.conf;prj_thread.local.conf"`.
- **Awaiting David:** C6 pin wiring for #7.
- **Decided:** ztoit public, Apache-2.0 (`README.md`); peripherals from Zephyr drivers, two SoCs
  (`docs/design/zephyr-port.md` addenda). Test programs use ≤8 tasks, not 100 (#3).
- **Found:** #2, #3, #5: P3. DNS skipped for now.
