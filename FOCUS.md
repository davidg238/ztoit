# Focus

- **Now:** branch `main`. nRF52840 dongle runs the Thread soak (#6); the C6 moves to I/O dev (#7: switch + BMP180
  via Zephyr APIs, `docs/design/zephyr-port.md` "peripherals" addendum).
- **Soak:** dongle since 2026-11-14 ~16:45, logger PID 216022 → `~/.local/state/ztoit/soak-nrf-20261114.log`.
  Read: `grep -a "soak: round\|FAILED" <log> | tail`; post on #6. Reopening the dongle's port does not reset it.
  C6 soak result (67 min) is on #4.
- **Boards:** dongle flashes with `tools/nrf-flash.sh` after a RESET press. C6: opening its serial port resets it;
  end captures on `ztoit: exit reason`. gw85224-01 runs `tcp-echo6` (user) and rete's `udp-echo6` (:7777).
  `rete-custos` stays disabled for the loan (`CLAUDE.md`). Thread build: `-DEXTRA_CONF_FILE="prj_thread.conf;prj_thread.local.conf"`.
- **Awaiting David:** nothing.
- **Decided:** ztoit public, Apache-2.0; peripherals from Zephyr drivers; port shape in `docs/design/zephyr-port.md`.
- **Found:** #2, #3 (hello now 8 tasks), #5: P3. DNS skipped for now.
