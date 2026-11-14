// ztoit: run one embedded Toit program as the boot program under Zephyr.
// The image is installed once into the flash registry (relocated to its mapped
// address, as toit's image writer primitives do) and runs from flash in place.
// A later boot with the same program id reuses the installed copy.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>

#include "top.h"
#include "flash_allocation.h"
#include "flash_registry.h"
#include "os.h"
#include "program.h"
#include "scheduler.h"
#include "snapshot.h"
#include "vm.h"
#include "third_party/dartino/object_memory.h"
#include "thread.h"

// The boot program's relocatable image, in flash (zephyr/CMakeLists.txt).
static const unsigned char program_image[] __aligned(4) = {
#include "program_image.inc"
};
static const unsigned program_image_len = sizeof(program_image);

namespace toit {

// The registry holds one program, at offset 0.
static const word PROGRAM_OFFSET = 0;

static const Program* install_program() {
  // The image is chunks of one relocation-bit word plus WORD_BIT_SIZE words;
  // the first chunk starts with the program header.
  const int CHUNK_WORD_SIZE = ImageOutputStream::CHUNK_SIZE;
  unsigned image_size = program_image_len;
  if (image_size % (CHUNK_WORD_SIZE * WORD_SIZE) != 0) FATAL("bad image size %u", image_size);
  word relocated_size = (image_size / CHUNK_WORD_SIZE) * WORD_BIT_SIZE;
  word allocation_size = Utils::round_up(relocated_size, FLASH_PAGE_SIZE);

  FlashAllocation::Header image_header(*reinterpret_cast<const FlashAllocation::Header*>(&program_image[WORD_SIZE]));
  const FlashAllocation* installed = FlashRegistry::allocation(PROGRAM_OFFSET);
  if (installed != null && installed->is_program() &&
      memcmp(installed->id(), image_header.id(), FlashAllocation::Header::ID_SIZE) == 0) {
    printf("ztoit: program %d B, already in flash\n", static_cast<int>(installed->size()));
    return static_cast<const Program*>(installed);
  }

  if (allocation_size > FlashRegistry::allocations_size()) FATAL("program %d B > registry", static_cast<int>(allocation_size));
  int64 start = k_uptime_get();
  if (FlashRegistry::erase_chunk(PROGRAM_OFFSET, allocation_size) == 0) FATAL("erase failed");
  ProgramImage image(FlashRegistry::region(PROGRAM_OFFSET, allocation_size), allocation_size);
  ImageOutputStream output(image);
  word buffer[WORD_BIT_SIZE];
  int image_word_size = image_size / WORD_SIZE;
  for (int i = 0; i < image_word_size; i += CHUNK_WORD_SIZE) {
    bool first = output.empty();
    word offset = FlashRegistry::offset(output.cursor());
    output.write(reinterpret_cast<word*>(const_cast<unsigned char*>(&program_image[i * WORD_SIZE])), CHUNK_WORD_SIZE, buffer);
    // The header goes last (commit below): only a complete write is valid.
    word skip = first ? sizeof(FlashAllocation::Header) : 0;
    if (!FlashRegistry::write_chunk(reinterpret_cast<uint8*>(buffer) + skip, offset + skip,
                                    WORD_BIT_SIZE * WORD_SIZE - skip)) {
      FATAL("flash write failed at %d", static_cast<int>(offset));
    }
  }
  uint8 metadata[FlashAllocation::Header::METADATA_SIZE] = { 0 };
  word program_size = image_header.size();
  const FlashAllocation::Header header(image.begin(), FLASH_ALLOCATION_TYPE_PROGRAM,
                                       image_header.id(), program_size, metadata);
  if (!FlashAllocation::commit(image.begin(), program_size, &header)) FATAL("commit failed");
  printf("ztoit: program %d B, installed in flash in %d ms\n",
         static_cast<int>(relocated_size), static_cast<int>(k_uptime_get() - start));
  return static_cast<const Program*>(FlashRegistry::allocation(PROGRAM_OFFSET));
}

static int run() {
  FlashRegistry::set_up();
  OS::set_up();
  ObjectMemory::set_up();
  Program* program = const_cast<Program*>(install_program());
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

// A USB CDC ACM console (nRF52840 dongle) drops output until the host opens
// the port, so wait for DTR, but at most 3 s: on a wall wart there is no host.
static void wait_for_console() {
#if DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart)
  const struct device* console = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
  uint32_t dtr = 0;
  for (int i = 0; i < 30 && !dtr; i++) {
    uart_line_ctrl_get(console, UART_LINE_CTRL_DTR, &dtr);
    k_msleep(100);
  }
#endif
}

int main() {
  wait_for_console();
#ifdef CONFIG_NET_L2_OPENTHREAD
  ztoit_thread_up();
#endif
  return toit::run();
}
