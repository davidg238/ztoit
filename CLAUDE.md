# ztoit — the Toit VM on Zephyr

Spike: run the Toit VM core (from `~/workspaceToit/toit-v2.0.0`) under Zephyr on ESP32-C6, reusing rete's backend seams (`~/workspace/rete/nodus/oevm/src/*_backend.h`).

@FOCUS.md

- The only C6 on msi is rete's bench node `c6spare` (custos holds `/dev/ttyUSB0`). Borrowing it means stopping custos first, then restoring the rete image and restarting custos.
