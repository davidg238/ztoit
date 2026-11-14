# Focus

- **Next session starts with (2026-11-14):** branch `main`. #1 steps 1–2 done (`spike/step2/README.md`): flash
  registry, XIP programs, Toit UDP over Thread. Focus moves to #4 (networking over Thread). First action: a
  Zephyr network service in `zephyr/toit/boot.toit` so `net.open` works (template `system/extensions/host/network.toit`).
- **Board state:** the C6 runs the Thread build (`udp_echo.toit`). Thread build: `west build -d <dir> --
  -DEXTRA_CONF_FILE="prj_thread.conf;prj_thread.local.conf" -DEXTRA_DTC_OVERLAY_FILE=thread.overlay`; the
  dataset file is gitignored (copy of rete's `prj_esp32c6_thread.local.conf`). `rete-custos` stays disabled (`CLAUDE.md`).
- **Awaiting David:** whether #1 closes now (VM core boots and runs XIP) or waits for the `system/` containers path.
- **Decided:** C6 first; VM core only; toit source `~/workspaceToit/toit-v2.0.0` branch `zephyr-port` (pushed to `fork`).
- **Decided:** port shape in `docs/design/zephyr-port.md` + its 2026-11-14 addendum (registry, XIP, zsock event source).
- **Found:** #2 (alloc rows faster than stock), #3 (hello OOM on 100 tasks) — P3, not chased.
