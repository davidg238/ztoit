#include "store.h"

#include <stdio.h>
#include <string.h>

#include <zephyr/fs/fcb.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/storage/flash_map.h>

#define STAGE_KEY "agent/stage"
#define APP_DIR "agent/app"
#define CONSOLE_KEY "agent/console"

bool store_init(void) {
  int err = settings_subsys_init();
  if (err != 0) printk("agent: settings unavailable (%d)\n", err);
  return err == 0;
}

// --- Staging partition ---------------------------------------------------------

#if FIXED_PARTITION_EXISTS(stage_partition)

// The board overlay's stage_partition (ESP32: 124 KB at 0x3E0000, where
// Zephyr's tables had scratch_partition before 4.5, #47). Not memory-mapped:
// read through the flash API.
static const struct flash_area* area;

static int area_read(void* ctx, size_t offset, void* buf, size_t len) {
  return flash_area_read(area, offset, buf, len);
}

static int area_write(void* ctx, size_t offset, const void* buf, size_t len) {
  return flash_area_write(area, offset, buf, len);
}

static int area_erase(void* ctx) {
  return flash_area_erase(area, 0, area->fa_size);
}

static struct stage_io io = {area_read, area_write, area_erase, 0, NULL};

const struct stage_io* store_staging(void) {
  if (area == NULL) {
    if (flash_area_open(FIXED_PARTITION_ID(stage_partition), &area) != 0) return NULL;
    io.size = area->fa_size;
  }
  return &io;
}

#else

const struct stage_io* store_staging(void) {
  return NULL;
}

#endif

// --- Console ring -----------------------------------------------------------------

#if PARTITION_EXISTS(log_partition)

// FCB over the board overlay's log_partition: an entry per line, and when the
// ring is full the oldest sector is erased to make room (fcb_rotate). Called
// with agent.c's console lock held, from the VM's print and the agent's push:
// nothing here prints.
// The partition's grandparent is its flash, whose erase block is a sector.
#define LOG_SECTORS \
  (PARTITION_SIZE(log_partition) / DT_PROP(DT_GPARENT(DT_NODELABEL(log_partition)), erase_block_size))
#define LOG_MAGIC 0x7a636f6e  // "zcon"
static struct flash_sector log_sectors[LOG_SECTORS];
static struct fcb log_fcb = {.f_magic = LOG_MAGIC, .f_sectors = log_sectors};
// An entry, its tail padded to the flash's write block (fcb's f_align).
static uint8_t log_buf[CONSOLE_ENTRY_MAX + 8] __aligned(8);

static int log_append(void* ctx, const void* entry, size_t len) {
  struct fcb_entry loc;
  int err = fcb_append(&log_fcb, len, &loc);
  if (err == -ENOSPC) {
    err = fcb_rotate(&log_fcb);
    if (err == 0) err = fcb_append(&log_fcb, len, &loc);
  }
  if (err != 0) return err;
  size_t padded = ROUND_UP(len, log_fcb.f_align);
  memcpy(log_buf, entry, len);
  memset(log_buf + len, log_fcb.f_erase_value, padded - len);
  err = flash_area_write(log_fcb.fap, FCB_ENTRY_FA_DATA_OFF(loc), log_buf, padded);
  if (err != 0) return err;
  return fcb_append_finish(&log_fcb, &loc);
}

static int log_walk(void* ctx, console_entry_fn fn, void* arg) {
  struct fcb_entry loc = {0};
  int err;
  while ((err = fcb_getnext(&log_fcb, &loc)) == 0) {
    if (loc.fe_data_len > CONSOLE_ENTRY_MAX) continue;  // not one of ours
    err = flash_area_read(log_fcb.fap, FCB_ENTRY_FA_DATA_OFF(loc), log_buf, loc.fe_data_len);
    if (err != 0) return err;
    if (!fn(arg, log_buf, loc.fe_data_len)) return 0;
  }
  return err == -ENOTSUP ? 0 : err;  // -ENOTSUP: past the last entry
}

static const struct console_ring log_ring = {log_append, log_walk, NULL};

// The partition held something else before (on the C6 and the ESP32, the
// default table's lpcore slots): FCB refuses sectors it did not write, so the
// partition is erased once and set up again.
const struct console_ring* store_console_ring(void) {
  uint32_t count = LOG_SECTORS;
  int err = flash_area_get_sectors(PARTITION_ID(log_partition), &count, log_sectors);
  if (err == 0) {
    log_fcb.f_sector_cnt = count;
    err = fcb_init(PARTITION_ID(log_partition), &log_fcb);
  }
  if (err == -ENOMSG) {
    printk("agent: log_partition is not a console ring: erased\n");
    const struct flash_area* area;
    err = flash_area_open(PARTITION_ID(log_partition), &area);
    if (err == 0) err = flash_area_erase(area, 0, area->fa_size);
    if (err == 0) err = fcb_init(PARTITION_ID(log_partition), &log_fcb);
  }
  if (err != 0) {
    printk("agent: console ring unavailable (%d): no remote console\n", err);
    return NULL;
  }
  return &log_ring;
}

#else

const struct console_ring* store_console_ring(void) {
  printk("agent: no log_partition: no remote console\n");
  return NULL;
}

#endif

// --- Records ----------------------------------------------------------------------

struct exact {
  void* out;
  size_t size;
  bool found;
};

static int load_exact(const char* key, size_t len, settings_read_cb read, void* cb_arg, void* param) {
  struct exact* e = param;
  if (key == NULL && len == e->size && read(cb_arg, e->out, len) == (ssize_t)len) e->found = true;
  return 0;
}

static bool load(const char* name, void* out, size_t size) {
  struct exact e = {out, size, false};
  settings_load_subtree_direct(name, load_exact, &e);
  return e.found;
}

bool store_stage_load(struct store_stage* out) {
  return load(STAGE_KEY, out, sizeof(*out)) && out->name[STORE_NAME_MAX] == 0;
}

int store_stage_save(const struct store_stage* stage) {
  return settings_save_one(STAGE_KEY, stage, sizeof(*stage));
}

int store_stage_clear(void) {
  return settings_delete(STAGE_KEY);
}

uint32_t store_console_acked_load(void) {
  uint32_t line = 0;
  if (!load(CONSOLE_KEY, &line, sizeof(line))) line = 0;
  return line;
}

int store_console_acked_save(uint32_t line) {
  return settings_save_one(CONSOLE_KEY, &line, sizeof(line));
}

static void app_key(char* key, size_t cap, const char* name) {
  snprintf(key, cap, APP_DIR "/%s", name);
}

bool store_app_load(const char* name, struct store_app* out) {
  char key[sizeof(APP_DIR) + 1 + STORE_NAME_MAX];
  app_key(key, sizeof(key), name);
  return load(key, out, sizeof(*out));
}

int store_app_save(const char* name, const struct store_app* app) {
  char key[sizeof(APP_DIR) + 1 + STORE_NAME_MAX];
  app_key(key, sizeof(key), name);
  return settings_save_one(key, app, sizeof(*app));
}

void store_app_unreport(const char* name) {
  struct store_app app;
  if (!store_app_load(name, &app) || !app.reported) return;
  app.reported = 0;
  store_app_save(name, &app);
}

// The records are walked by name, one load pass per record: the callback only
// reads, so fn may save or delete. Apps are few.
struct next {
  const char* after;
  char name[STORE_NAME_MAX + 1];
  bool found;
};

static int find_next(const char* key, size_t len, settings_read_cb read, void* cb_arg, void* param) {
  struct next* n = param;
  if (key == NULL || strlen(key) > STORE_NAME_MAX || strcmp(key, n->after) <= 0) return 0;
  if (!n->found || strcmp(key, n->name) < 0) {
    strcpy(n->name, key);
    n->found = true;
  }
  return 0;
}

static bool next_name(const char* after, char* out) {
  struct next n = {.after = after};
  settings_load_subtree_direct(APP_DIR, find_next, &n);
  if (n.found) strcpy(out, n.name);
  return n.found;
}

void store_apps_each(store_app_fn fn, void* ctx) {
  char name[STORE_NAME_MAX + 1] = "";
  struct store_app app;
  while (next_name(name, name)) {
    if (store_app_load(name, &app)) fn(name, &app, ctx);
  }
}

void store_apps_clear(void) {
  char name[STORE_NAME_MAX + 1] = "";
  char key[sizeof(APP_DIR) + 1 + STORE_NAME_MAX];
  while (next_name(name, name)) {
    app_key(key, sizeof(key), name);
    settings_delete(key);
  }
}
