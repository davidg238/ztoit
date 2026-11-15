# Step 4 — STIC on three CPUs (2026-11-14)

Toit v2.0.0 on Zephyr 4.2.0, same `performance.toit`, 160 KB Toit heap, no radio. Raw output beside this file
(the RP2350B capture's first lines were lost to a manual reset and restored from the rerun's console).

| Test (s) | ESP32-C6, RISC-V 160 MHz, flash (XIP) | RP2350B, Cortex-M33 150 MHz, RAM | nRF52840, Cortex-M4F 64 MHz, flash (XIP) |
|--:|--:|--:|--:|
| alloc | 9.6 | 9.7 | 31.6 |
| array write | 26.4 | 22.1 | 87.0 |
| dictionary write | 4.0 | 7.3 | 12.9 |
| float math | 31.4 | 32.7 | 91.9 |
| integer math | 13.7 | 11.8 | 44.6 |
| collection iterate | 17.7 | 17.5 | 57.7 |
| collection write | 7.0 | 10.1 | 23.9 |
| string compare | 2.0 | 1.6 | 6.3 |

- C6 column: step 2 (`spike/step2/`). RP2350B: WeAct RP2350B core (rete's board, `-DBOARD_ROOT`), program in
  the RAM registry (no Zephyr flash driver for the RP2350's QMI). nRF52840: dongle, program in its flash registry.
- Per MHz the M4F dongle is about level with the C6 (×2.5 clock, ×2.9–3.3 time); the RP2350B is level with the
  C6 except dictionary and collection write (1.8×, 1.4×), not yet measured.
