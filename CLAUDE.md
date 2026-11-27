# ztoit — the Toit VM on Zephyr

Spike: run the Toit VM core (from `~/workspaceToit/toit-v2.0.0`) under Zephyr on ESP32-C6, reusing rete's backend seams (`~/workspace/rete/nodus/oevm/src/*_backend.h`).

@FOCUS.md

- Which session owns which msi board: `~/workspace/board-alloc.md` (David's; read it before touching a port). Returning rete's c6spare 6e60: write_flash 0 `~/.local/state/ztoit/c6spare-full-20261114.bin`, then `systemctl --user enable --now rete-custos`.
- Thread builds need `-DEXTRA_CONF_FILE="prj_thread.conf;prj_thread.local.conf"`; without it every net.open throws UNSUPPORTED (#22).
- nRF52840 dongle on msi USB: flash with `tools/nrf-flash.sh` after a RESET press (no debugger). For full JTAG, David has a Segger J-Link and other probes — ask.
- Agent builds add `prj_agent.conf` and need the git-ignored local conf's `CONFIG_TOIT_AGENT_PORTA`/`_PUBKEY` (#31).
- Rerun `tools/fork-sdk.sh` after any fork commit: node and host tool must report the same `tools/sdk-id.sh` string (#24).
- Name serial ports by `/dev/serial/by-id/…`: the ttyUSB numbers move when a board is replugged. Exception: the two EzSBC WROOM-32s share USB serial `015E0000`, so by-id points at whichever enumerated last; use `/dev/serial/by-path/…` for them.
- One Zephyr tree, `~/zephyr-4.5` (SDK 1.0.1, its own `.venv`; `source env.sh`). Always set `ZEPHYR_BASE`: unset, CMake picks the NCS Zephyr (#42). nRF boards need `patches/zephyr-nrf-802154-hfclk-ready-deferred.patch` applied in that tree (#23). `~/zephyr-esp32` (4.2) is rete's, not ztoit's (#47).
- A rebuild of the Toit images gives them new ids, and the boot rule then erases installed apps on the next flash (porta's goals reinstall them). To keep them, reuse the old build's `toit/*.image` (not across a fork change that adds a primitive module: #58).
