// ztoit: run Toit's container system under Zephyr. The registry holds the
// fork's Zephyr system program at offset 0 and each container after it, each
// installed once (relocated to its mapped address, as toit's image writer
// primitives do) and run from flash in place. A boot whose registry already
// holds exactly these images reuses them; otherwise the registry is erased and
// all are reinstalled (a dev bootstrap until the agent owns containers).
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

// A relocatable image in flash (zephyr/CMakeLists.txt). flags is the
// allocation's metadata[0]: ContainerService.FLAG-RUN-BOOT for containers.
struct Image {
  const unsigned char* data;
  unsigned size;
  uint8_t flags;
};
#include "images.inc"
static const int IMAGE_COUNT = sizeof(images) / sizeof(images[0]);

namespace toit {

// The image is chunks of one relocation-bit word plus WORD_BIT_SIZE words.
static const int CHUNK_WORD_SIZE = ImageOutputStream::CHUNK_SIZE;

static word relocated_size(const Image& image) {
  if (image.size % (CHUNK_WORD_SIZE * WORD_SIZE) != 0) FATAL("bad image size %u", image.size);
  return (image.size / CHUNK_WORD_SIZE) * WORD_BIT_SIZE;
}

static word allocation_size(const Image& image) {
  return Utils::round_up(relocated_size(image), FLASH_PAGE_SIZE);
}

// The first chunk starts with the program header.
static FlashAllocation::Header image_header(const Image& image) {
  return *reinterpret_cast<const FlashAllocation::Header*>(&image.data[WORD_SIZE]);
}

static bool installed_at(const Image& image, word offset) {
  const FlashAllocation* installed = FlashRegistry::allocation(offset);
  return installed != null && installed->is_program() &&
         memcmp(installed->id(), image_header(image).id(), FlashAllocation::Header::ID_SIZE) == 0;
}

static const Program* install_image(const unsigned char* data, unsigned size, word offset, uint8 flags) {
  Image source = { data, size, flags };
  word relocated = relocated_size(source);
  word allocation = allocation_size(source);
  int64 start = k_uptime_get();
  ProgramImage image(FlashRegistry::region(offset, allocation), allocation);
  ImageOutputStream output(image);
  word buffer[WORD_BIT_SIZE];
  int image_word_size = size / WORD_SIZE;
  for (int i = 0; i < image_word_size; i += CHUNK_WORD_SIZE) {
    bool first = output.empty();
    word chunk_offset = FlashRegistry::offset(output.cursor());
    output.write(reinterpret_cast<word*>(const_cast<unsigned char*>(&data[i * WORD_SIZE])), CHUNK_WORD_SIZE, buffer);
    // The header goes last (commit below): only a complete write is valid.
    word skip = first ? sizeof(FlashAllocation::Header) : 0;
    if (!FlashRegistry::write_chunk(reinterpret_cast<uint8*>(buffer) + skip, chunk_offset + skip,
                                    WORD_BIT_SIZE * WORD_SIZE - skip)) {
      FATAL("flash write failed at %d", static_cast<int>(chunk_offset));
    }
  }
  uint8 metadata[FlashAllocation::Header::METADATA_SIZE] = { 0 };
  metadata[0] = flags;
  FlashAllocation::Header source_header = image_header(source);
  const FlashAllocation::Header header(image.begin(), FLASH_ALLOCATION_TYPE_PROGRAM,
                                       source_header.id(), source_header.size(), metadata);
  if (!FlashAllocation::commit(image.begin(), source_header.size(), &header)) FATAL("commit failed");
  printf("ztoit: program %d B at %d, installed in the registry in %d ms\n",
         static_cast<int>(relocated), static_cast<int>(offset), static_cast<int>(k_uptime_get() - start));
  return static_cast<const Program*>(FlashRegistry::allocation(offset));
}

// Installs images[] back to back from offset 0, unless the registry holds
// exactly them already. Returns the system program, image 0.
static const Program* install_images() {
  word total = 0;
  for (int i = 0; i < IMAGE_COUNT; i++) total += allocation_size(images[i]);
  if (total > FlashRegistry::allocations_size()) {
    FATAL("images %d B > registry %d B", static_cast<int>(total), static_cast<int>(FlashRegistry::allocations_size()));
  }
  bool current = total == FlashRegistry::allocations_size() || FlashRegistry::allocation(total) == null;
  for (word i = 0, offset = 0; current && i < IMAGE_COUNT; offset += allocation_size(images[i]), i++) {
    current = installed_at(images[i], offset);
  }
  if (current) {
    printf("ztoit: %d images, already in the registry\n", IMAGE_COUNT);
  } else {
    if (FlashRegistry::erase_chunk(0, FlashRegistry::allocations_size()) == 0) FATAL("erase failed");
    for (word i = 0, offset = 0; i < IMAGE_COUNT; offset += allocation_size(images[i]), i++) {
      install_image(images[i].data, images[i].size, offset, images[i].flags);
    }
  }
  return static_cast<const Program*>(FlashRegistry::allocation(0));
}

static int run() {
  FlashRegistry::set_up();
  OS::set_up();
  ObjectMemory::set_up();
  Program* program = const_cast<Program*>(install_images());
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
  return toit::run();
}
