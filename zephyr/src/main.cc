// ztoit: run one embedded Toit program as the boot program under Zephyr.
// Mirrors toit's toit_run_image.cc: relocate the image into page-aligned
// memory, then run it on a fresh VM.
#include <algorithm>
#include <cstdio>
#include <zephyr/kernel.h>

#include "top.h"
#include "flash_registry.h"
#include "os.h"
#include "program.h"
#include "scheduler.h"
#include "snapshot.h"
#include "vm.h"
#include "third_party/dartino/object_memory.h"

extern "C" {
  extern unsigned char program_image[];
  extern unsigned int program_image_len;
}

namespace toit {

static Program* relocate_program() {
  // The image is chunks of one relocation-bit word plus WORD_BIT_SIZE words.
  const int CHUNK_WORD_SIZE = WORD_BIT_SIZE + 1;
  unsigned image_size = program_image_len;
  if (image_size % (CHUNK_WORD_SIZE * WORD_SIZE) != 0) FATAL("bad image size %u", image_size);
  int relocated_size = (image_size / CHUNK_WORD_SIZE) * WORD_BIT_SIZE;
  auto memory = _new AlignedMemory(relocated_size, TOIT_PAGE_SIZE);
  if (memory == null || memory->address() == null) FATAL("no memory for %d B program", relocated_size);
  ProgramImage relocated(memory->address(), relocated_size);
  ImageOutputStream output(relocated);
  int image_word_size = image_size / WORD_SIZE;
  for (int i = 0; i < image_word_size; i += CHUNK_WORD_SIZE) {
    int chunk_word_size = std::min(i + CHUNK_WORD_SIZE, image_word_size) - i;
    output.write(reinterpret_cast<word*>(&program_image[i * WORD_SIZE]), chunk_word_size);
  }
  printf("ztoit: program image %u B, relocated %d B\n", image_size, relocated_size);
  return reinterpret_cast<Program*>(relocated.program());
}

static int run() {
  FlashRegistry::set_up();
  OS::set_up();
  ObjectMemory::set_up();
  Program* program = relocate_program();
  Scheduler::ExitState exit;
  {
    VM vm;
    vm.load_platform_event_sources();
    int group_id = vm.scheduler()->next_group_id();
    exit = vm.scheduler()->run_boot_program(program, group_id);
  }
  printf("ztoit: exit reason %d value %d\n", exit.reason, static_cast<int>(exit.value));
  return 0;
}

}  // namespace toit

int main() {
  return toit::run();
}
