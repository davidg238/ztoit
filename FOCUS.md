# Focus

- **Next session starts with (2026-11-14):** branch `main`, focus #4. `net.open`, UDP and TCP work over Thread
  (#4 comments). First action: an echo soak (UDP and TCP) over the mesh, then decide whether DNS/AAAA is needed.
  Serial capture resets the board on open; end captures on `ztoit: exit reason`, never on `ECHO OK` (the C smoke
  test prints that first).
- **Board state:** the C6 runs the Thread build (`udp_echo.toit`); gw85224-01 runs user unit `tcp-echo6` (:7777). Thread build: `west build -d <dir> --
  -DEXTRA_CONF_FILE="prj_thread.conf;prj_thread.local.conf" -DEXTRA_DTC_OVERLAY_FILE=thread.overlay`; the
  dataset file is gitignored (copy of rete's `prj_esp32c6_thread.local.conf`). Programs compile with the
  fork-lib SDK (`tools/fork-sdk.sh`, `TOIT_HOST` in `env.sh`). `rete-custos` stays disabled (`CLAUDE.md`).
- **Awaiting David:** nothing. #1 closed 2026-11-14.
- **Decided:** C6 first; VM core only; toit source `~/workspaceToit/toit-v2.0.0` branch `zephyr-port` (pushed to `fork`).
- **Decided:** port shape in `docs/design/zephyr-port.md` + its 2026-11-14 addendum (registry, XIP, zsock event source).
- **Found:** #2 (alloc rows faster than stock), #3 (hello OOM on 100 tasks), #5 (radio driver nesting) — P3.
