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
- **Stand-ins:** `flash_registry_zephyr.cc` is an empty registry; `open/read/write/close/lseek` return ENOSYS;
  64-bit atomics are spinlock-guarded (RV32 has none).
- **Primitive modules** missing from the build link silently as empty and fail at runtime with
  `PRIMITIVE_LOOKUP_FAILED m:n` — add the `resources/*.cc` that defines module m.
