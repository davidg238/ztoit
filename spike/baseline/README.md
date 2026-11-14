# Step 0 — stock Toit v2.0.0 on ESP32-C6 (c6spare), 2026-11-14

SDK and envelope v2.0.0 (`~/.local/state/ztoit/sdk`); `hello.toit` installed as a boot container; log in `serial.txt`.

| Measure | Value |
|---|---|
| App image at 0x10000 (IDF + VM + system 175 KB + hello 89 KB) | 1,689,008 B |
| System free heap, idle | 239,112 B (largest block 176,128) |
| Process heap, idle | 928 B allocated / 8,192 reserved |
| With 100 sleeping tasks | 75,404 B allocated / 81,920 reserved, about 745 B per task |
| System free with 100 tasks | 153,208 B |
