# ztoit — the Toit VM on Zephyr

Spike: run the Toit VM core (from `~/workspaceToit/toit-v2.0.0`) under Zephyr on ESP32-C6, reusing rete's backend seams (`~/workspace/rete/nodus/oevm/src/*_backend.h`).

@FOCUS.md

- The C6 on msi (`/dev/ttyUSB0`, rete's c6spare, MAC 40:4c:ca:44:6e:60) is ztoit's from 2026-11-14 for a few days, and `rete-custos` is stopped and disabled. Its rete image is backed up in full at `~/.local/state/ztoit/c6spare-full-20261114.bin` (sha256 baa5710c…). To hand it back: write_flash 0 that file, then `systemctl --user enable --now rete-custos`.
- Thread builds need `-DEXTRA_CONF_FILE="prj_thread.conf;prj_thread.local.conf"`; without it every net.open throws UNSUPPORTED (#22).
- nRF52840 dongle on msi USB: flash with `tools/nrf-flash.sh` after a RESET press (no debugger). For full JTAG, David has the ocm + acc modules and a Segger J-Link — ask.
