// ztoit: run Toit's container system under Zephyr. The registry holds the
// fork's Zephyr system program at offset 0 and the firmware's containers after
// it (images.inc), then what was installed at run time: the agent's images,
// installed here from staging before the VM starts, and the system program's
// own allocations. Each image is relocated to its mapped address (as toit's
// image writer primitives do) and run from flash in place.
//
// Boot rule (nodus-toit design, addendum 2026-11-17): if the firmware images
// sit exactly at the front of the registry, everything after them is kept;
// otherwise the whole registry and the agent's install records are erased and
// the firmware images reinstalled. Installed apps then come back from porta's
// goals, which are the record of what should be on the node.
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

#ifdef CONFIG_TOIT_AGENT
#include "agent/store.h"
extern "C" void agent_start(void);
#endif

// A relocatable image in flash (zephyr/CMakeLists.txt). flags is the
// allocation's metadata[0].
struct Image {
  const unsigned char* data;
  unsigned size;
  uint8_t flags;
};
static const uint8_t FLAG_RUN_BOOT = 1;  // ContainerService.FLAG-RUN-BOOT: the containers
#include "images.inc"
static const int IMAGE_COUNT = sizeof(images) / sizeof(images[0]);

namespace toit {

// The image is chunks of one relocation-bit word plus WORD_BIT_SIZE words.
static const int CHUNK_WORD_SIZE = ImageOutputStream::CHUNK_SIZE;
static const int CHUNK_BYTES = CHUNK_WORD_SIZE * WORD_SIZE;

// An image's bytes: a firmware image's array, or the staging partition, which
// is not memory-mapped and is read a chunk at a time.
struct ImageSource {
  word size;
  bool (*read)(const void* ctx, word offset, void* buffer, word length);
  const void* ctx;
};

static bool read_array(const void* ctx, word offset, void* buffer, word length) {
  memcpy(buffer, static_cast<const uint8*>(ctx) + offset, length);
  return true;
}

static ImageSource firmware_source(const Image& image) {
  return { static_cast<word>(image.size), read_array, image.data };
}

static bool is_chunked(word size) {
  return size > 0 && size % CHUNK_BYTES == 0;
}

static word allocation_size(word image_size) {
  return Utils::round_up((image_size / CHUNK_WORD_SIZE) * WORD_BIT_SIZE, FLASH_PAGE_SIZE);
}

// The first chunk starts with the program header.
struct HeaderBytes {
  alignas(word) uint8 bytes[sizeof(FlashAllocation::Header)];
  const FlashAllocation::Header* get() const { return reinterpret_cast<const FlashAllocation::Header*>(bytes); }
};

static bool read_header(const ImageSource& source, HeaderBytes* out) {
  return source.read(source.ctx, WORD_SIZE, out->bytes, sizeof(out->bytes));
}

static bool installed_at(const Image& image, word offset) {
  const FlashAllocation* installed = FlashRegistry::allocation(offset);
  HeaderBytes header;
  return installed != null && installed->is_program() && read_header(firmware_source(image), &header) &&
         memcmp(installed->id(), header.get()->id(), FlashAllocation::Header::ID_SIZE) == 0;
}

// Writes the image at offset, header last (commit): only a complete write is
// valid. Answers the program, or null if the source or the flash failed.
static const Program* install_image(const ImageSource& source, word offset, uint8 flags) {
  word relocated = (source.size / CHUNK_WORD_SIZE) * WORD_BIT_SIZE;
  word allocation = allocation_size(source.size);
  int64 start = k_uptime_get();
  ProgramImage image(FlashRegistry::region(offset, allocation), allocation);
  ImageOutputStream output(image);
  word chunk[CHUNK_WORD_SIZE];
  word buffer[WORD_BIT_SIZE];
  for (word i = 0; i < source.size; i += CHUNK_BYTES) {
    if (!source.read(source.ctx, i, chunk, CHUNK_BYTES)) return null;
    bool first = output.empty();
    word chunk_offset = FlashRegistry::offset(output.cursor());
    output.write(chunk, CHUNK_WORD_SIZE, buffer);
    word skip = first ? sizeof(FlashAllocation::Header) : 0;
    if (!FlashRegistry::write_chunk(reinterpret_cast<uint8*>(buffer) + skip, chunk_offset + skip,
                                    WORD_BIT_SIZE * WORD_SIZE - skip)) {
      printf("ztoit: flash write failed at %d\n", static_cast<int>(chunk_offset));
      return null;
    }
  }
  HeaderBytes source_header;
  if (!read_header(source, &source_header)) return null;
  uint8 metadata[FlashAllocation::Header::METADATA_SIZE] = { 0 };
  metadata[0] = flags;
  const FlashAllocation::Header header(image.begin(), FLASH_ALLOCATION_TYPE_PROGRAM,
                                       source_header.get()->id(), source_header.get()->size(), metadata);
  if (!FlashAllocation::commit(image.begin(), source_header.get()->size(), &header)) {
    printf("ztoit: commit failed at %d\n", static_cast<int>(offset));
    return null;
  }
  printf("ztoit: program %d B at %d, installed in the registry in %d ms\n",
         static_cast<int>(relocated), static_cast<int>(offset), static_cast<int>(k_uptime_get() - start));
  return static_cast<const Program*>(FlashRegistry::allocation(offset));
}

// The boot rule (top of file). Answers the system program, image 0, and in
// *end the offset after the firmware images.
static const Program* install_images(word* end) {
  word total = 0;
  for (int i = 0; i < IMAGE_COUNT; i++) {
    if (!is_chunked(images[i].size)) FATAL("bad image size %u", images[i].size);
    total += allocation_size(images[i].size);
  }
  if (total > FlashRegistry::allocations_size()) {
    FATAL("images %d B > registry %d B", static_cast<int>(total), static_cast<int>(FlashRegistry::allocations_size()));
  }
  bool current = true;
  for (word i = 0, offset = 0; current && i < IMAGE_COUNT; offset += allocation_size(images[i].size), i++) {
    current = installed_at(images[i], offset);
  }
  if (current) {
    printf("ztoit: %d images, already in the registry\n", IMAGE_COUNT);
  } else {
    if (FlashRegistry::erase_chunk(0, FlashRegistry::allocations_size()) == 0) FATAL("erase failed");
#ifdef CONFIG_TOIT_AGENT
    store_apps_clear();
    printf("ztoit: registry erased: installed apps come back from porta's goals\n");
#endif
    for (word i = 0, offset = 0; i < IMAGE_COUNT; offset += allocation_size(images[i].size), i++) {
      if (install_image(firmware_source(images[i]), offset, images[i].flags) == null) FATAL("install failed");
    }
  }
  *end = total;
  return static_cast<const Program*>(FlashRegistry::allocation(0));
}

#ifdef CONFIG_TOIT_AGENT

static bool read_staging(const void* ctx, word offset, void* buffer, word length) {
  const struct stage_io* io = static_cast<const struct stage_io*>(ctx);
  return io->read(io->ctx, offset, buffer, length) == 0;
}

// Drops the allocation an app record names, if it is still that program, as
// the Toit registry frees one: its header page erased.
static void erase_app(const struct store_app& app) {
  const FlashAllocation* allocation = FlashRegistry::allocation(app.offset);
  if (allocation != null && allocation->is_program() &&
      memcmp(allocation->id(), app.id, FlashAllocation::Header::ID_SIZE) == 0) {
    FlashRegistry::erase_chunk(app.offset, FLASH_PAGE_SIZE);
  }
}

// The first run of free pages at or after from that holds size bytes, or -1.
static word find_free(word from, word size) {
  word limit = FlashRegistry::allocations_size();
  word offset = from;
  while (offset + size <= limit) {
    const FlashAllocation* allocation = FlashRegistry::allocation(offset);
    if (allocation != null) {
      offset += Utils::round_up(allocation->size(), FLASH_PAGE_SIZE);
      continue;
    }
    word end = offset + FLASH_PAGE_SIZE;
    while (end - offset < size && FlashRegistry::allocation(end) == null) end += FLASH_PAGE_SIZE;
    if (end - offset >= size) return offset;
    offset = end;
  }
  return -1;
}

// Refused here, the agent does not fetch the same hash again.
static void refuse_staged(struct store_stage* stage, const char* why) {
  printf("ztoit: staged %s not installed: %s\n", stage->name, why);
  stage->state = STAGE_REFUSED;
  store_stage_save(stage);
}

// A verified staged image goes into the first free run after the firmware
// images, replacing the allocation of the same name. Its record says
// installed only after the commit, so a power cut repeats this from staging.
static void install_staged(word from) {
  const struct stage_io* io = store_staging();
  struct store_stage stage;
  if (io == null || !store_stage_load(&stage) || stage.state != STAGE_VERIFIED) return;
  if (!is_chunked(stage.size)) return refuse_staged(&stage, "not a relocatable image");
  struct store_app app;
  if (store_app_load(stage.name, &app) && app.state != APP_ERASED) erase_app(app);
  word size = allocation_size(stage.size);
  word offset = find_free(from, size);
  if (offset < 0) return refuse_staged(&stage, "no room in the registry");
  if (FlashRegistry::erase_chunk(offset, size) == 0) return refuse_staged(&stage, "erase failed");
  const Program* program = install_image(ImageSource{ static_cast<word>(stage.size), read_staging, io }, offset,
                                         FLAG_RUN_BOOT);
  if (program == null) return refuse_staged(&stage, "install failed");
  memset(&app, 0, sizeof(app));
  memcpy(app.hash, stage.hash, sizeof(app.hash));
  app.size = stage.size;
  app.offset = offset;
  memcpy(app.id, program->id(), sizeof(app.id));
  app.keep = stage.keep;
  app.state = APP_INSTALLED;
  store_app_save(stage.name, &app);
  store_stage_clear();
  printf("ztoit: installed %s from staging\n", stage.name);
}

// keep = false: an image runs at the boot after its install, and is erased at
// the one after that.
static void run_once(const char* name, struct store_app* app, void* ctx) {
  if (app->state == APP_RAN) {
    erase_app(*app);
    app->state = APP_ERASED;
    app->reported = 0;  // the agent pushes obs/apps/<name> #erased
    store_app_save(name, app);
    printf("ztoit: %s ran once (keep false): erased\n", name);
  } else if (app->state == APP_INSTALLED && !app->keep) {
    app->state = APP_RAN;
    store_app_save(name, app);
  }
}

#endif  // CONFIG_TOIT_AGENT

static int run() {
  FlashRegistry::set_up();
  OS::set_up();
  ObjectMemory::set_up();
#ifdef CONFIG_TOIT_AGENT
  bool records = store_init();
#endif
  word firmware_end;
  Program* program = const_cast<Program*>(install_images(&firmware_end));
#ifdef CONFIG_TOIT_AGENT
  if (records) {
    install_staged(firmware_end);
    store_apps_each(run_once, NULL);
  }
  agent_start();
#endif
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
