# ztoit — the Toit VM on Zephyr

A port spike: the [Toit](https://toitlang.org) v2.0.0 virtual machine running as a Zephyr application,
first on the ESP32-C6. The VM changes live in the Toit fork's
[`zephyr-port`](https://github.com/davidg238/toit/tree/zephyr-port) branch; this repo holds the Zephyr module
(`modules/toit/`), the application (`zephyr/`), and the measurements (`spike/`). The backend seams come from
[rete](https://github.com/davidg238/rete).

## What works (ESP32-C6, 2026-11-14)

| Step | Result | Record |
|---|---|---|
| 0 | Stock Toit v2.0.0 baseline on the same board | `spike/baseline/` |
| 1 | VM core boots under Zephyr; STIC benchmark at least as fast as stock on every row | `spike/step1/` |
| 2 | Programs installed in a flash registry and run in place (XIP); UDP over IPv6 on Thread | `spike/step2/` |
| 3 | `net.open` provider; TCP over IPv6 on Thread | `zephyr/toit/tcp_echo.toit` |

Design decisions: `docs/design/zephyr-port.md`. Known issues and open work: GitHub issues.

## Build

Needs the Zephyr SDK 0.17.4, the [Toit SDK v2.0.0](https://github.com/toitlang/toit/releases/tag/v2.0.0) for the
host compiler, and an ESP32-C6 board.

    west init -m https://github.com/davidg238/ztoit ztoit-ws && cd ztoit-ws && west update
    west blobs fetch hal_espressif
    export ZEPHYR_BASE=$PWD/zephyr TOIT_SRC=$PWD/toit
    # Unpack the Toit SDK to ~/.local/state/ztoit/sdk/toit, then build the host compiler with the fork's lib/:
    export TOIT_HOST=$(ztoit/tools/fork-sdk.sh)
    west build -b esp32c6_devkitc/esp32c6/hpcore -d build ztoit/zephyr -- -DPROGRAM=$PWD/ztoit/zephyr/toit/hello.toit
    west flash -d build

Thread: copy `zephyr/prj_thread.local.conf.example` to `prj_thread.local.conf`, fill in your network's
dataset, and add `-DEXTRA_CONF_FILE="prj_thread.conf;prj_thread.local.conf" -DEXTRA_DTC_OVERLAY_FILE=thread.overlay`.

## License

ztoit's own code is Apache-2.0 (`LICENSE`), matching Zephyr. The Toit sources it builds keep their licenses:
the VM is LGPL-2.1 and the Toit libraries are MIT (see the Toit fork).
