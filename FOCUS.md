# Focus

- **Next session starts with (2026-11-14):** branch `main`, focus #1. Step 1 done (`spike/step1/README.md`). Next: real
  `flash_registry_zephyr.cc` over a flash partition, and program images run from flash (XIP) instead of a RAM copy.
  First action: find the Zephyr esp32c6 flash-mmap API and the storage partition in the board DTS.
- **Board state:** the C6 runs stock v2.0.0 + STIC; `cd zephyr && west flash -d build` restores ztoit.
  `rete-custos` is stopped and disabled for the loan (hand-back steps in `CLAUDE.md`).
- **Awaiting David:** nothing.
- **Decided:** C6 first; VM core only; toit source `~/workspaceToit/toit-v2.0.0` branch `zephyr-port` (pushed to `fork`).
- **Decided:** port shape in `docs/design/zephyr-port.md` (k_mutex/k_condvar, static page arena, minimal boot.toit).
- **Found:** allocation-heavy rows faster than stock — #2 (P3), not to be chased in #1.
