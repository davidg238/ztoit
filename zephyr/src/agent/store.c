#include "store.h"

#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/storage/flash_map.h>

#define STAGE_KEY "agent/stage"
#define APP_DIR "agent/app"

bool store_init(void) {
  int err = settings_subsys_init();
  if (err != 0) printk("agent: settings unavailable (%d)\n", err);
  return err == 0;
}

// --- Staging partition ---------------------------------------------------------

#if FIXED_PARTITION_EXISTS(scratch_partition)

// The board's scratch_partition (ESP32 default table: unused without MCUboot
// swap). Not memory-mapped: read through the flash API.
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
    if (flash_area_open(FIXED_PARTITION_ID(scratch_partition), &area) != 0) return NULL;
    io.size = area->fa_size;
  }
  return &io;
}

#else

const struct stage_io* store_staging(void) {
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
