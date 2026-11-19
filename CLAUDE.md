# ztoit — the Toit VM on Zephyr

Spike: run the Toit VM core (from `~/workspaceToit/toit-v2.0.0`) under Zephyr on ESP32-C6, reusing rete's backend seams (`~/workspace/rete/nodus/oevm/src/*_backend.h`).

@FOCUS.md

- The C6 on msi (`/dev/ttyUSB0`, rete's c6spare, MAC 40:4c:ca:44:6e:60) is ztoit's from 2026-11-14 for a few days, and `rete-custos` is stopped and disabled. Its rete image is backed up in full at `~/.local/state/ztoit/c6spare-full-20261114.bin` (sha256 baa5710c…). To hand it back: write_flash 0 that file, then `systemctl --user enable --now rete-custos`.
- Thread builds need `-DEXTRA_CONF_FILE="prj_thread.conf;prj_thread.local.conf"`; without it every net.open throws UNSUPPORTED (#22).
- nRF52840 dongle on msi USB: flash with `tools/nrf-flash.sh` after a RESET press (no debugger). For full JTAG, David has the ocm + acc modules and a Segger J-Link — ask.
- Agent builds add `prj_agent.conf` and need the git-ignored local conf's `CONFIG_TOIT_AGENT_PORTA`/`_PUBKEY` (#31).
- Rerun `tools/fork-sdk.sh` after any fork commit: node and host tool must report the same `tools/sdk-id.sh` string (#24).
- Name serial ports by `/dev/serial/by-id/…`: the ttyUSB numbers move when a board is replugged. Exception: the two EzSBC WROOM-32s share USB serial `015E0000`, so by-id points at whichever enumerated last; use `/dev/serial/by-path/…` for them.
- Two Zephyr trees: `~/zephyr-esp32` (4.2, SDK 0.17.4) for the existing boards, `~/zephyr-4.5` (SDK 1.0.1, own `.venv`) for the nRF54L15. Always set `ZEPHYR_BASE` (and `ZEPHYR_SDK_INSTALL_DIR` for 4.5); unset, CMake picks the NCS Zephyr (#42).
- A rebuild of the Toit images gives them new ids, and the boot rule then erases installed apps on the next flash (porta's goals reinstall them). To keep them, reuse the old build's `toit/*.image`.
