# ztoit's Toit fork and upstream Toit: where they differ

Started 2026-11-26. Fork: toit `wip-plan2-fix-a` (pushed as `zephyr-port`), based on upstream `master` of
2026-10-31 (`315ea422`). Upstream has its own Zephyr port open as toitlang/toit #3322–#3333; compared here at
`bdc3e3fe` (the base of #3397).

## Decision

- **ztoit maintains its own fork.** Fixes to the VM, compiler and core libraries come across from upstream
  `master` by `git cherry-pick -x`; the Zephyr layer is ztoit's own.
- **Hardware comes from Zephyr's devicetree and drivers**, not from drivers rewritten in Toit (zephyr-port.md,
  addendum 2026-11-14 "peripherals come from Zephyr"). That is the main reason the two ports differ in shape.
- **Several SoC families, one port.** nRF first, ESP32-C6 second; the WROOM-32 is kept while it costs little.
- **Name collisions with upstream's port are not worked on** until that port is on upstream `master`. Cost: if
  it lands, the module name `zephyr` must be settled (see "Collisions") before the next pick that touches it.

## Side by side

| Seam | Upstream port | ztoit fork | Why ztoit differs |
|---|---|---|---|
| Targets | nRF5340 (the flash backend asserts it) | nRF52840, nRF54L15, ESP32-C6, ESP32; RAM registry where no flash driver | several SoCs is a goal |
| Build | Zephyr app inside the Toit repo (`toolchains/zephyr`), newlib | Zephyr module + app in ztoit, picolibc | one board package per hardware revision lives in ztoit |
| Platform macros | `TOIT_ZEPHYR` plus a new `TOIT_EMBEDDED` | `TOIT_ZEPHYR` from `__ZEPHYR__`; `TOIT_FREERTOS` reused as "embedded" | fewer edits to shared files |
| Current thread | `k_thread_custom_data` | from `k_current_get()` (`Thread::current` in `os_zephyr.cc`) | Espressif's WiFi adapter owns the custom-data slot |
| Thread stacks | `k_thread_stack_alloc` (`CONFIG_DYNAMIC_THREAD`) | `aligned_alloc` at `Z_KERNEL_STACK_OBJ_ALIGN` | no second kernel heap; valid while `CONFIG_USERSPACE` is off |
| Toit heap pages | `memalign` from the libc heap; GC range = all SRAM | static arena with a page map; GC range = the arena | GC metadata sized to the arena; the arena can sit in the ESP32's second DRAM bank |
| Real time | software offset over uptime | `sys_clock_settime` | |
| Flash registry writes | word at a time, never reprograms a written word, journaled page rewrite, recovery at boot | `flash_write`; erase only dirty pages; cache flush on Espressif | see "Reviewed" |
| Event sources | timer + one Zephyr source | timer, keys, sockets or the BLE-UDP relay | |
| Peripherals | Toit's `gpio`, `uart`, `i2c`, `spi`, `adc`, `pwm`, watchdog, reset reasons, BLE | one `zephyr` module over devicetree devices: sensors, LEDs, keys, mic, LC3, UART RX, speech, observe | the decision above |
| Networking | none | TCP, UDP, WiFi, Thread, UDP over BLE | |
| Mbed TLS | Toit's own copy; threading, time and entropy hooks in `os_zephyr.cc` | Zephyr's module | |
| Deployment | firmware envelopes, deploy over the debug probe | agent over CoAP (nodus-toit design), faber | |

## Collisions

Same path, different content: `src/os_zephyr.cc`, `src/flash_registry_zephyr.cc`, `src/vm_zephyr.cc`,
`src/compiler/propagation/type_primitive_zephyr.cc`, `system/extensions/zephyr/boot.toit`. Same name, different
meaning: `TOIT_ZEPHYR` (and the `mutex.h` branches), `MODULE_ZEPHYR` in `primitive.h`, and `import zephyr`
(upstream `lib/zephyr/zephyr.toit` holds reset reasons; ztoit's `lib/zephyr.toit` is the device API every app
imports). The file collisions resolve as "keep ours". The module name is the one with a cost: a rename touches
the primitive module, the library and one import line per app.

**2026-11-27: renamed.** ztoit's module and library are now `ztoit` (`MODULE_ZTOIT`, `lib/ztoit.toit`, `import ztoit`),
so `MODULE_ZEPHYR` and `import zephyr` no longer collide. The file collisions and `TOIT_ZEPHYR` remain. Text above
this line, and older design docs, say `zephyr` for the same module.

## Reviewed 2026-11-26

**Flash writes on nRF: sound as built.** Upstream's journal exists because Nordic's NVMC limits how often a word
may be programmed between erases, and a rewrite in place would exceed it. In the fork no word is programmed
twice:
- A program install erases its pages, writes the body past the header, then writes the header once
  (`write_image_chunk`, `FlashAllocation::commit`). `allocate` and `write_non_header_pages` follow the same order.
- The only rewrite-in-place path is `region_write`, and the Zephyr system program installs no storage service
  (`system/extensions/zephyr/boot.toit`), so no app can reach it.
- Zephyr's `soc_flash_nrf` driver refuses a write that is not word-aligned unless
  `CONFIG_SOC_FLASH_NRF_EMULATE_ONE_BYTE_WRITE_ACCESS` is set, and ztoit does not set it: a misaligned write is
  an error, not a second programming of a word.
- Condition: before flash regions (`storage.Region`) are offered on an NVMC part, `write_chunk` needs upstream's
  guard or an equivalent. The nRF54L15's RRAM has no such limit.

**Thread lookup: sound as built, with one sharp edge.** `Thread::current` is called only by `Locker`
(`mutex.cc`). Every `Locker` in the Zephyr sources runs on a thread Toit spawned or on the system thread; the
callbacks that run on Zephyr's threads (input, L2CAP receive, net_mgmt, the UART ISR) use atomics, `k_mutex` and
semaphores. The reason for not using custom data still holds on Zephyr 4.5 (`esp_adapter.c` uses it). The edge:
a `Locker` taken on a thread Toit did not spawn would read a `ThreadData` that is not there instead of failing
cleanly. Upstream's custom-data lookup fails with a FATAL in that case.

## Tests

The Toit test suite does not run on the port. What runs: `tools/test-host.sh` (BLE codecs) and
`zephyr/src/agent/test/run.sh` (agent). The Toit host suite does run on the fork (`ninja -C build/host
check_slow`, under two minutes): it covers the VM, compiler and libraries the fork shares with upstream, and
none of the Zephyr layer.

## Picks from upstream

Branch `upstream-picks-20261126` at `9aedce8b` (worktree `~/workspaceToit/toit-v2.0.0-picks`), 2026-11-26: 32
upstream commits picked with `-x` in upstream order, plus one fork commit. Merged into `wip-plan2-fix-a`
as `6cd05d94` the same day.

| Upstream PR | What | Note for the port |
|---|---|---|
| #3318 | bounds and type checks in primitives and fast paths | edits files the fork also edits; applied clean |
| #3342 | length of the wide non-local return bytecode | length table only; bytecode and image formats unchanged |
| #3289 | EC618 peripherals, shared watchdog module | taken because #3348 does not apply without it; adds modules `one_wire` and `watchdog` before `zephyr`; also types integer literals independently of the host's word size |
| #3350 | lost wakeup when cancelling a task that is processing messages | |
| #3341, #3348 | type propagator: non-local branches, precise primitive types | system image 4.2 KB smaller; the fork's rules ported in `2bc0e9f2` |
| #3346, #3349, #3352, #3394 | tests, LSP rename, test wrapper | host only |
| #3361, #3375, #3382 | smi arithmetic: MIN / -1, subtraction overflow, wrapping without undefined behaviour | |
| #3372–#3374, #3376–#3379, #3381, #3383–#3393 | message, encoder, TISON, UTF-8 and finalizer hardening | #3384 and #3387 concern 32-bit targets with a 64-bit host |

Skipped: #3319, #3343, #3351, #3369 (ESP-IDF BLE, CI, ethernet).

Evidence: the host suite fails the same 3 tests on the picks as on `ba2fef2d` (socat and valgrind not installed,
one multicast timeout) and passes the 59 tests the picks add. An nRF52840 Thread build and a C6 Thread + agent
build compile with no new warnings; flash is 2.7 KB and 4.0 KB smaller than the same builds from `ba2fef2d`.

Consequences of merging:
- **The `zephyr` module's number moves**, because `M(zephyr)` stays last in `primitive.h` and two modules now sit
  before it. An image compiled by the older SDK calls the wrong primitives on the newer VM: every image is
  rebuilt, installed apps included, and `tools/fork-sdk.sh` is rerun. This recurs whenever upstream adds a module.
- **The stock v2.0.0 binary in the SDK keeps the old length** for the wide non-local return. Only
  `tools/snapshot.toit` reads that table (decoding and disassembly), not image conversion.
- **An image from the older SDK still installs.** On the bench porta's goal re-installed an app compiled before
  the merge and it ran, because it calls no `zephyr` primitive. Nothing refuses such an image at install.

Bench, C6 on Thread with the agent, merged fork: boot, both images installed, Thread attach, echo, pull and
push, then a fetch, verify, reboot and install of an app from porta, which ran. The host suite on the merge
commit fails the same 3 tests.
