# Step 1 — Toit VM core on Zephyr, STIC on the C6 (2026-11-14)

Same board (c6spare, 160 MHz), same benchmark (`performance_stic/performance.toit`), Toit v2.0.0 both sides.
ztoit: `west build -- -DPROGRAM=…/performance.toit`, boot program `zephyr/toit/boot.toit`, image relocated into RAM.
Stock: SDK v2.0.0 `firmware-esp32c6.envelope`, STIC as a boot container. Raw output in the two `.txt` files.

| Test (s, mean of 3) | ztoit | stock v2.0.0 |
|--:|--:|--:|
| alloc | 9.1 | 20.3 |
| array write | 25.7 | 25.7 |
| dictionary write | 3.6 | 4.4 |
| float math | 30.8 | 56.1 |
| integer math | 13.2 | 13.5 |
| collection iterate | 17.3 | 17.7 |
| collection write | 6.7 | 9.2 |
| string compare | 1.9 | 1.9 |

Interpreter-bound rows match; allocation-heavy rows are 1.2–2.2× faster on ztoit, cause not yet measured.
