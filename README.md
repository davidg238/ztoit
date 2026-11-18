# ztoit — the Toit VM on Zephyr

A port spike: the [Toit](https://toitlang.org) v2.0.0 virtual machine running as a Zephyr application.

**What the spike shows:**

- **The Toit interpreter runs on Zephyr**, on four SoCs (weakest first):

  | SoC (CPU) | Link shown | IP | Toit heap | RAM | C heap | Program space | Runs from flash | Installs over the air |
  |---|---|---|---|---|---|---|---|---|
  | ESP32 WROOM-32 (Xtensa LX6) | WiFi | IPv4 | 48 KB | 192 KB + 96 KB, two banks | ~99 KB | 2.2 MB | yes | yes |
  | ESP32-C6 (RISC-V) | Thread | IPv6 | 160 KB | 427 KB, one region | ~90 KB | 4 MB | yes | yes |
  | nRF52840 (Cortex-M4) | Thread | IPv6 | 80 KB | 256 KB, one region | ~72 KB | 240 KB | yes | no |
  | RP2350B (Cortex-M33) | none | — | 160 KB | 520 KB, one region | ~197 KB | 128 KB, in RAM | no | no |

  - "Link shown" is what ztoit has run on the board, not everything the chip has: the C6 also has WiFi, which
    ztoit has not built. A build carries one link, and its sockets take that link's family.
  - "Toit heap" is the arena Toit objects and task stacks live in, a build setting (`CONFIG_TOIT_HEAP_SIZE`).
    "C heap" is the RAM left after the firmware's static data, thread stacks, the Toit heap and, on the
    WROOM-32, WiFi's own 44 KB heap; Toit's thread stacks and garbage-collector tables come out of it.
  - The WROOM-32 is the tightest: its second 96 KB bank is fully assigned to the thread stacks and the Toit
    heap (about 4 KB unassigned), so the Toit heap cannot grow much. 100 sleeping tasks do not fit in it (#3).
  - Measured 2026-11-17 on the builds in use: WiFi + agent (WROOM-32), Thread + agent (C6), the Thread soak
    (nRF52840 dongle), the benchmark build (RP2350B).

  Stock Toit needed one VM change, in the flash registry; the rest is new Zephyr platform code, plus the
  `zephyr` primitive module (appended after the stock modules, so a forked compiler is needed for now).
- **Programs run from flash in place (XIP)**, installed in a flash registry (the RP2350B installs into RAM:
  Zephyr has no flash driver for its QSPI controller yet).
- **IPv6 UDP and TCP over OpenThread** from Toit's `net` library, on the C6 and the nRF52840; the nRF52840
  dongle runs a UDP + TCP echo soak on a wall wart.
- **IPv4 over WiFi** on the WROOM-32: the station associates on the first `net.open` and disconnects after
  the last close; `zephyr/toit/wifi_echo.toit` does a UDP echo each cycle. The image holds no WiFi credentials.
- **Containers installed over the air**, on the WROOM-32 (WiFi) and the C6 (Thread): a C agent checks in with
  a gateway over CoAP, fetches a signed image, verifies it (SHA-256, ed25519) and installs it at the next
  boot, beside the containers built into the firmware (#31).
- **At least as fast as stock Toit** on a simple benchmark ([STIC](https://github.com/davidg238/performance_stic), same ESP32-C6, v2.0.0 both sides):
  interpreter-bound rows match, allocation-heavy rows are 1.2–2.2× faster (cause not yet measured, #2).
  STIC on all three CPUs: `spike/step4/`.
- **Peripherals come from Zephyr's drivers**, named by devicetree label, not Toit rewrites: `import zephyr`
  (`Sensor`, `Led`, `LedStrip`, `Key`), one primitive module generic over Zephyr's device classes (#7). On the
  C6: a BMP180 over I2C, the BOOT button (debounced input events), an output pin and the on-board RGB LED;
  BOOT and the RGB LED need no wiring.

Not yet: DNS, uninstall, the console sent to the gateway, raw `gpio`/`i2c`/`spi`; known issues are on GitHub.

The VM changes live in the Toit fork's
[`zephyr-port`](https://github.com/davidg238/toit/tree/zephyr-port) branch; this repo holds the Zephyr module
(`modules/toit/`), the application (`zephyr/`), and the measurements (`spike/`). The backend seams come from
[rete](https://github.com/davidg238/rete).

## What works (2026-11-17)

| Step | Result | Record |
|---|---|---|
| 0 | Stock Toit v2.0.0 baseline on the same board | [`spike/baseline/`](spike/baseline/) |
| 1 | VM core boots under Zephyr; STIC benchmark at least as fast as stock on every row | [`spike/step1/`](spike/step1/) |
| 2 | Programs installed in a flash registry and run in place (XIP); UDP over IPv6 on Thread | [`spike/step2/`](spike/step2/) |
| 3 | `net.open` provider; TCP over IPv6 on Thread | [`zephyr/toit/tcp_echo.toit`](zephyr/toit/tcp_echo.toit) |
| 4 | Second SoC: nRF52840 dongle runs Toit from its flash registry, UDP + TCP over Thread (soak) | [#6](https://github.com/davidg238/ztoit/issues/6) |
| 5 | Third SoC: RP2350B (RAM registry); STIC on all three CPUs | [`spike/step4/`](spike/step4/) |
| 6 | Zephyr devices from Toit: BMP180, BOOT key, output pin, RGB LED on the C6 | [`zephyr/toit/io.toit`](zephyr/toit/io.toit), [#7](https://github.com/davidg238/ztoit/issues/7) |
| 7 | Fourth SoC: ESP32 WROOM-32 over WiFi (IPv4), credentials from the settings store | [#25](https://github.com/davidg238/ztoit/issues/25), [#30](https://github.com/davidg238/ztoit/issues/30) |
| 8 | Signed containers installed over the air, WiFi and Thread; run-once and replace | [#31](https://github.com/davidg238/ztoit/issues/31) |

![ESP32-C6 running io.toit: BMP180, LED on GPIO3, on-board RGB LED](docs/img/c6-io.jpg)

Design decisions: `docs/design/zephyr-port.md`. Known issues and open work: GitHub issues.

## Build

Needs the Zephyr SDK 0.17.4, the [Toit SDK v2.0.0](https://github.com/toitlang/toit/releases/tag/v2.0.0) for the
host compiler, and an ESP32-C6 board, an ESP32 (WROOM-32) board, an nRF52840 dongle or an RP2350B board.

    west init -m https://github.com/davidg238/ztoit ztoit-ws && cd ztoit-ws && west update
    west blobs fetch hal_espressif
    export ZEPHYR_BASE=$PWD/zephyr TOIT_SRC=$PWD/toit
    # Unpack the Toit SDK to ~/.local/state/ztoit/sdk/toit, then build the fork's compiler (it knows the
    # `zephyr` primitives; needs cmake and ninja) into an SDK with the fork's lib/:
    export TOIT_HOST=$(ztoit/tools/fork-sdk.sh)
    west build -b esp32c6_devkitc/esp32c6/hpcore -d build ztoit/zephyr -- -DCONTAINERS="$PWD/ztoit/zephyr/toit/hello.toit"
    west flash -d build

nRF52840 dongle: `west build -b nrf52840dongle/nrf52840 -d build-nrf ztoit/zephyr`, press the dongle's RESET
button, then `ztoit/tools/nrf-flash.sh build-nrf` (needs `nrfutil` with `nrf5sdk-tools`). Console is USB CDC ACM;
the app waits up to 3 s for the host to open it, then runs anyway.

RP2350B (WeAct core, rete's out-of-tree board): `west build -b weact_rp2350b_core/rp2350b/m33 -d build-rp
ztoit/zephyr -- -DBOARD_ROOT=<rete>/nodus/oevm/zephyr`, then copy `build-rp/zephyr/zephyr.uf2` to the board in
BOOTSEL mode. Console is USB CDC ACM. The RNG is Zephyr's non-cryptographic timer generator (benchmarks only).

Thread: copy `zephyr/prj_thread.local.conf.example` to `prj_thread.local.conf`, fill in your network's
dataset, and add `-DEXTRA_CONF_FILE="prj_thread.conf;prj_thread.local.conf"` (C6 or nRF52840). The node joins on
the first `net.open`, and after the last close stays attached as a sleepy end device
(`-DCONFIG_TOIT_THREAD_DOWN_DETACH=y`: disables Thread instead).

WiFi (WROOM-32, `-b esp32_devkitc/esp32/procpu`): add `-DEXTRA_CONF_FILE="prj_wifi.conf"`. The image carries no
network name or passphrase: the node connects to the one network in its settings store (Zephyr's
`wifi_credentials`, in the `storage` partition, which an app flash leaves alone), written at flash time by a
host tool (#30). With the store empty, `net.open` throws `FILE_NOT_FOUND`. DHCP, IPv4.

Agent (optional, WiFi or Thread): add `prj_agent.conf` and a git-ignored local conf (`prj_wifi.local.conf` or
`prj_thread.local.conf`, see the `.example` files) with the gateway's address and its public signing key. At
each `net.open` the agent checks in over CoAP; a signed container image named by the gateway is fetched into a
staging partition, verified (SHA-256, ed25519), and installed into the registry at the next boot (#31;
`docs/design/2026-11-15-nodus-toit-design.md`). Shown on the WROOM-32 over WiFi and the ESP32-C6 over Thread.
The host tool must build images with the same SDK the firmware reports: `tools/sdk-id.sh` prints that identity
(`v2.0.0+zephyr.<fork commit>`), and `tools/fork-sdk.sh` stamps it into `$TOIT_HOST version`, so rerun it after
changing the fork.

## License

ztoit's own code is Apache-2.0 (`LICENSE`), matching Zephyr. The Toit sources it builds keep their licenses:
the VM is LGPL-2.1 and the Toit libraries are MIT (see the Toit fork).
