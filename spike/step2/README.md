# Step 2 — flash registry, XIP programs, Toit UDP over Thread (2026-11-14)

Board c6spare (160 MHz). Raw output beside this file.

**Flash registry.** `toit_partition` = upper 4 MB of the 8 MB flash (`zephyr/boards/*.overlay`), mapped with
`spi_flash_mmap`. Boot installs the embedded image once (relocated to its mapped address, header committed last)
and later boots with the same program id reuse it: hello `installed in flash in 91 ms`, then `already in flash`.

**STIC, program in flash (XIP) vs step 1 (RAM copy), s:**

| alloc | array write | dict write | float | integer | coll. iterate | coll. write | string cmp |
|--:|--:|--:|--:|--:|--:|--:|--:|
| 9.6 / 9.1 | 26.4 / 25.7 | 4.0 / 3.6 | 31.4 / 30.8 | 13.7 / 13.2 | 17.7 / 17.3 | 7.0 / 6.7 | 2.0 / 1.9 |

XIP costs 2–11% (bytecode and literals fetched through the flash cache); still faster than stock on every row.

**Thread.** `prj_thread.conf` (+ gitignored `prj_thread.local.conf` dataset, + `thread.overlay`): the C6 attaches
to the porta OTBR as an MTD child in 3.7 s. `toit/udp_echo.toit` sends 5 datagrams over IPv6 to gw85224-01's
`udp-echo6` (:7777): `UDP ECHO OK 5/5`, 15–35 ms round trip (`thread-udp-echo.txt`; gateway journal agrees).
