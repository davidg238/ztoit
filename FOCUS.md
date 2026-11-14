# Focus

- **Next session starts with (2026-11-14):** branch `main`, focus #4 (networking over Thread). `net.open` + UDP
  work on the mesh (#4 comment). First action: `tcp_zephyr.cc` on `ZsockEventSource`, shaped like `tcp_linux.cc`;
  test with a TCP echo on gw85224-01 (none runs there yet; only `udp-echo6` :7777).
- **Board state:** the C6 runs the Thread build (`udp_echo.toit`). Thread build: `west build -d <dir> --
  -DEXTRA_CONF_FILE="prj_thread.conf;prj_thread.local.conf" -DEXTRA_DTC_OVERLAY_FILE=thread.overlay`; the
  dataset file is gitignored (copy of rete's `prj_esp32c6_thread.local.conf`). Programs compile with the
  fork-lib SDK (`tools/fork-sdk.sh`, `TOIT_HOST` in `env.sh`). `rete-custos` stays disabled (`CLAUDE.md`).
- **Awaiting David:** nothing. #1 closed 2026-11-14.
- **Decided:** C6 first; VM core only; toit source `~/workspaceToit/toit-v2.0.0` branch `zephyr-port` (pushed to `fork`).
- **Decided:** port shape in `docs/design/zephyr-port.md` + its 2026-11-14 addendum (registry, XIP, zsock event source).
- **Found:** #2 (alloc rows faster than stock), #3 (hello OOM on 100 tasks) — P3, not chased.
