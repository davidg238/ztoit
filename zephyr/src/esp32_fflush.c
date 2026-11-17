// ESP32 only. Espressif's ROM linker script (esp32.rom.newlib-funcs.ld) assigns
// fflush to ROM newlib's, which faults in its __getreent on a picolibc FILE:
// every Toit print flushes (primitive_core.cc). picolibc's stdout on Zephyr is
// unbuffered, each character going to the console, so a flush has nothing to do.
// Linked with -Wl,--wrap=fflush (CMakeLists.txt); rete #521.
#include <stdio.h>

int __wrap_fflush(FILE* stream) {
  (void)stream;
  return 0;
}
