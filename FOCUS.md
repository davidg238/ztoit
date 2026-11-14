# Focus

- **Next session starts with (2026-11-15):** branch `main`, focus #4. Overnight soak running since 2026-11-14 15:28.
  First action: `grep -a "soak: round\|FAILED" ~/.local/state/ztoit/soak-overnight-20261114.log | tail`, post the
  result on #4, then `kill` the logger (PID 170400) before flashing anything. Then decide whether DNS/AAAA is needed.
- **Board state:** the C6 runs `soak.toit`; gw85224-01 runs user unit `tcp-echo6` (:7777). Opening the serial port
  resets the board; end captures on `ztoit: exit reason`, never on `ECHO OK` (the C smoke test prints that first).
  Thread build: `west build -d <dir> -- -DEXTRA_CONF_FILE="prj_thread.conf;prj_thread.local.conf"
  -DEXTRA_DTC_OVERLAY_FILE=thread.overlay` (dataset file gitignored). Programs compile with `tools/fork-sdk.sh`'s
  SDK (`TOIT_HOST` in `env.sh`). `rete-custos` stays disabled for the loan (`CLAUDE.md`).
- **Awaiting David:** nothing.
- **Decided:** toit source `~/workspaceToit/toit-v2.0.0` branch `zephyr-port`, pushed to `fork` over SSH.
- **Decided:** port shape in `docs/design/zephyr-port.md` + 2026-11-14 addendum (registry, XIP, zsock event source,
  net.open provider, TCP TX pool 64). #1 closed.
- **Found:** #2 (alloc rows faster than stock), #3 (hello OOM on 100 tasks), #5 (radio driver nesting): P3.
