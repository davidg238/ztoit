# Focus

- **Next session starts with:** branch `main`. Plan 1 (containers) is done (#10). Write the plan for design §11
  step 2, the network provider up/down (`docs/design/2026-11-15-nodus-toit-design.md`), with superpowers:writing-plans.
- **Boards:** C6 (`/dev/ttyUSB0`) runs hello+crash+io as containers. Dongle 2dbf (BLE EC:34:E5:7C:A8:E1) runs
  the soak as a container, logger PID 688963 → `soak-nrf-2dbf-container-20261115.log`. a6b3 silent since 12:54
  (#12, second occurrence), logger 632379 still attached. Reflashes need David's go. `rete-custos` stays disabled.
- **Awaiting David:** XIAO nRF54L15 Sense (due week of 11-16).
- **Decided:** containers stay stock Toit; the fork's `system/extensions/zephyr/boot.toit` is the system program.
  Boot rule: the registry is erased unless it holds exactly the built images (image ids are deterministic, #10);
  install (§11 step 4) must replace it. `-DCONTAINERS=a.toit;b.toit` replaces `-DPROGRAM`.
- **#9:** next test (summary without `--gc`) needs a free dongle. **#12:** dongles go silent after network
  degradation; P2.
- **Found:** #2, #3, #5, #13, #14–#17: P3. #8, #12: P2.
