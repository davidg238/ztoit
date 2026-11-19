# The Toit VM on Zephyr — port decisions

Code: toit `zephyr-port` branch (`src/*_zephyr.cc`); ztoit `modules/toit/` (Zephyr module), `zephyr/` (app).

- **Platform macros.** `__ZEPHYR__` → `TOIT_ZEPHYR`, tested before `ESP_PLATFORM` (Zephyr's Espressif HAL defines it).
  `TOIT_FREERTOS` is also defined, meaning "embedded device"; FreeRTOS headers are guarded `!TOIT_ZEPHYR`.
- **Kernel primitives.** `Mutex` = `k_mutex`, `ConditionVariable` = `k_condvar`, current `Thread*` in k_thread
  custom data. Stacks from the libc heap at `Z_KERNEL_STACK_OBJ_ALIGN` (valid while `CONFIG_USERSPACE` is off) —
  no second fixed kernel heap.
- **Toit heap pages** come from one static `sys_heap` arena (`CONFIG_TOIT_HEAP_SIZE`), so
  `OS::get_heap_memory_range` is exact. Its 160 KB default is a guess awaiting data.
- **Time.** Monotonic = `k_uptime_ticks`; real time = `sys_clock_gettime/settime`. No POSIX layer.
- **mbedtls** is Zephyr's module (3.6.4); `CONFIG_MBEDTLS_RSA_C` is enabled only to get `BIGNUM_C`.
- **Boot.** A bare app cannot be the boot program: `print`'s service discovery is an RPC to the system process.
  `zephyr/toit/boot.toit` is a minimal system (toit's `SystemServiceManager` in-process, trace printing) that
  calls the app's `main`. The full `system/` + flash registry + bundled containers is the later path.
- **Stand-ins:** `open/read/write/close/lseek` return ENOSYS;
  64-bit atomics are spinlock-guarded (RV32 has none).
- **Primitive modules** missing from the build link silently as empty and fail at runtime with
  `PRIMITIVE_LOOKUP_FAILED m:n` — add the `resources/*.cc` that defines module m.

## Addendum 2026-11-14 — flash registry, XIP, sockets

- **Registry** = devicetree `toit_partition` (upper 4 MB), `spi_flash_mmap`ed; writes and erases go through
  Zephyr's flash API, whose Espressif driver keeps the mapped view coherent (`flush()` is empty, as on ESP32).
  The mmap call is Espressif-specific until another SoC needs the registry.
- **Programs run in place.** `main.cc` installs the embedded image at registry offset 0 if no valid program with
  its id is there. One program, no `system/` containers yet. The device UUID is toit's fixed host constant
  (no `getenv` on Zephyr), stable across boots, so installed programs stay valid.
- **Sockets.** `ZsockEventSource` is the `zsock_poll` counterpart of `EpollEventSource`. zsock_poll is
  level-triggered, so an fd's events are disarmed once dispatched and re-armed by the primitive that next gets
  EAGAIN; the Toit monitor processes that notification after its `clear-state`, so no wakeup is lost. An
  eventfd wakes the poll on register, re-arm and close; only the poll thread closes fds.
- **UDP is IPv6-only** (`udp_zephyr.cc`): 16-byte addresses; the library's 4-byte `#[0,0,0,0]` bind means `::`.
- **`net.open`** is served by `ZephyrNetworkProvider` in `boot.toit` (toit's `network_base.toit`), `PROXY-NONE`
  like the host: sockets run in the client. `network.address` is the OMR address, so the Thread layer enables
  `CONFIG_OPENTHREAD_SLAAC` (without it the node has only mesh-local and link-local addresses).
- **Library changes** live on the fork's `lib/` (IPv6 `IpAddress.parse`, `Client.address`/`resolve` on IPv6-only
  stacks). Programs compile with `tools/fork-sdk.sh`'s SDK: stock v2.0.0 binaries + the fork's `lib/`.
- **TCP** (`tcp_zephyr.cc`) uses the same event source and re-arm rule. Zephyr sizes TCP's send window from the
  TX buffer pool (count × 128 B / 3), so the Thread layer sets `CONFIG_NET_BUF_TX_COUNT=64` (window 2730): at the
  default 16 a 1000 B write is accepted and never sent. RX and packet counts stay default (re-measured 5/5).
- **Test echo servers** on gw85224-01, port 7777: rete's `udp-echo6` (system unit) and ztoit's `tcp-echo6` (user
  unit, `tools/gateway/`).

## Addendum 2026-11-14 — peripherals come from Zephyr, not Toit

ztoit targets Zephyr only; the interpreter is the product, drivers are the Zephyr community's. Toit's
rewrite-every-driver-in-Toit model is not followed here.

- **Wiring is devicetree.** Toit names a device by devicetree node label or alias, never by pin number.
- **Sensors:** one generic primitive over Zephyr's sensor API (fetch + channel read); any Zephyr sensor driver
  works with no per-device code. First: `bosch,bmp180`.
- **Inputs** (switches, buttons): Zephyr's input subsystem (`gpio-keys`, its debounce) delivers events into the
  Toit event loop.
- **Raw `gpio`/`i2c`/`spi`** primitives are thin shims over Zephyr's APIs: an escape hatch, and existing Toit
  driver packages can run on them. No ESP-IDF resource code, no Toit driver for a part Zephyr already supports.
- **Cost:** a part Zephyr lacks (e.g. VL6180) needs a Zephyr driver (upstreamable) or the raw-bus escape hatch.

## Addendum 2026-11-14 — second SoC (nRF52840 dongle)

- **Registry on memory-mapped flash.** Supersedes "mmap is Espressif-specific": `spi_flash_mmap` only under
  `CONFIG_SOC_FAMILY_ESPRESSIF_ESP32`; elsewhere the partition is read at the flash node's address + offset.
- **Dongle layout** (`zephyr/boards/nrf52840dongle_nrf52840.overlay`): app 0x1000–0xa0000 (`FLASH_LOAD_SIZE` makes
  overflow a link error), `toit_partition` 0xa0000–0xdc000, storage (OpenThread settings), nRF5 bootloader.
- **The program image is const, in flash** (`generate_inc_file_for_target`): it is read once, to install. As
  RAM `.data` it cost 67 KB and starved the libc heap (thread stacks), stalling the dongle after Thread attach.
- **Radio enable lives in the board overlay**; driver Kconfig follows the devicetree, so a Thread build is
  `prj_thread.conf` on any board.
- **USB CDC console:** `main` waits ≤3 s for DTR, then runs (a wall wart has no host). The dongle's Open
  bootloader enters DFU only from its RESET pin; GPREGRET 0xB1 did not work.
- **Third SoC (RP2350B, 2026-11-14).** Zephyr 4.2 has no flash driver for the RP2350's QMI, so without a
  `toit_partition` (or `CONFIG_FLASH`) the registry is a static 128 KB RAM array: programs install at boot and
  are not XIP. No entropy driver either: the board conf uses the non-cryptographic timer RNG (benchmarks only).

## Addendum 2026-11-14 — the `zephyr` primitive module and the fork compiler (#7)

Implements "peripherals come from Zephyr" above.

- **Own module, appended last.** `M(zephyr, MODULE_ZEPHYR)` is the final entry in `MODULES` (`src/primitive.h`),
  so every stock module keeps its number. Implemented in `src/resources/zephyr.cc`; other platforms get the
  weak null entry. API: `lib/zephyr.toit` (`Sensor`, `LedStrip`, `Led`, `Key`).
- **Fork compiler.** The compiler maps `#primitive.m.p` to (module, index) at compile time, so a stock
  compiler rejects a new module. `tools/fork-sdk.sh` builds the fork's `toit.compile` (≈30 s) into the SDK;
  the rest of the SDK stays stock.
- **Per device class, not per driver.** Ten primitives cover sensor, LED, LED strip and input; a new driver in
  those classes is devicetree only. Devices by node label (`CONFIG_DEVICE_DT_METADATA`); `gpio-leds`/`gpio-keys`
  children, which are not devices, from tables built from the devicetree.
- **Keys:** `INPUT_CALLBACK_DEFINE` runs on Zephyr's input thread, which is not a Toit thread and must not take
  Toit locks (it aborted with "thread must be present"). The callback marks the key pending and gives a
  semaphore; `ZephyrKeyEventSource`'s own thread dispatches. Only built with `CONFIG_INPUT`.
- **Bare-board test.** The C6's BOOT button and on-board WS2812 (`ws2812-i2s`, data line only) need no wiring.
  `spi2` is disabled: the board dts puts it on GPIO6/7, `i2c0`'s pins.
- **Driver quirk:** Zephyr's `bosch,bmp180` reports temperature on `DIE_TEMP`, not `AMBIENT_TEMP`.

## Addendum 2026-11-17: a patched Zephyr for nRF Thread (#23)

- **Zephyr carries one local patch,** `patches/zephyr-nrf-802154-hfclk-ready-deferred.patch`, applied by hand
  in the west workspace (`git -C $ZEPHYR_BASE apply`); nothing applies it automatically yet. Without it an nRF
  board whose HF crystal is already running (USB, on the dongle) panics on the first transmit from a sleeping
  radio, which the always-sleepy node of design §7 does at once.
- **Why:** `nrf_802154_clock_hfclk_start()` is asynchronous by contract, but with the clock already on Zephyr's
  glue gets the onoff notification inside the request, and the open-source radio scheduler then changes the
  driver's state in the middle of `nrf_802154_core_cca`. The patch delivers that notification from a `k_timer`
  once it can take the driver's critical section. A one-tick deferral alone is not enough (it still panicked).
- **Cost:** the patch declares two driver-internal functions by hand, and upstream `main` has the same glue, so
  it must be carried until it is fixed upstream.
- **Debug options** (off by default): `-DFATAL_DUMP=ON` reports a fatal error at the next boot (the USB CDC
  console cannot print one), `-DHFCLK_HOLD=ON` reproduces the dongle's clock condition on a board without USB.

## Addendum 2026-11-17: the mapped registry needs a real flush on Espressif (#31, #33)

- **Found:** `FlashAllocation::commit` failed 2 of 3 over-the-air installs on the WROOM-32 while the header was
  in flash. A boot-time reproducer (read a page through the mapping, write it, read again) gave 32 of 32 pages
  stale on the WROOM-32 and on the ESP32-C6.
- **Cause, as far as measured:** `esp_mmu_paddr_find_caps()` answers not-found for the registry's
  `spi_flash_mmap` mapping, so the HAL's write skips its cache flush. Why the lookup misses is open (#33).
- **Fix (fork `50f0be99`, `7fb2838e`):** `FlashRegistry::flush()` calls `cache_sync()` on the ESP32 and
  `cache_hal_invalidate_addr()` over the mapping on the other Espressif SoCs; 0 of 32 stale after.
- **Invalidates** the earlier comment in `flash_registry_zephyr.cc` that Espressif's write keeps the mapped view
  coherent. Earlier installs passed because the pages had not been read before they were written.

## Addendum 2026-11-19: a second Zephyr, v4.5.0, for the nRF54L15 (#41)

- **Decision (David, 2026-11-19): upgrade, not backport.** v4.5.0 defines `xiao_nrf54l15`. It lives in its own
  west workspace, `~/zephyr-4.5`, with SDK 1.0.1 (its `SDK_VERSION`) and its own venv. SDK 1.0.1 is not
  registered with CMake (`setup.sh` without `-c`), so 4.2 builds keep SDK 0.17.4.
- **`~/zephyr-esp32` (4.2 plus the #23 patch) stays** for the C6, WROOM-32 and dongle until each is re-proven on 4.5.
- Every build sets `ZEPHYR_BASE` explicitly: unset, CMake resolves to the NCS Zephyr (#42).
