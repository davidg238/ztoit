// ESP32 WiFi only. Espressif's WiFi blob (ieee80211_sta_new_state) zeroes a
// static ETSTimer (the power-save wake-null timer) and calls ets_timer_setfn
// on it at every connect, never ets_timer_done. hal_espressif's
// ets_timer_legacy.c then creates a fresh esp_timer each time, and the one the
// slot held is orphaned: 32 B of the system heap per connect, ~2 h of 30 s
// WiFi cycles to exhaust it (power save off does not help). rete #521.
//
// The guard remembers each slot's esp_timer. A set-up that finds the slot
// zeroed while a handle is still held stops and deletes that handle first;
// ets_timer_done forgets it. Linked with -Wl,--wrap (CMakeLists.txt).
// Measured on the WROOM-32 only: 15 min, 26 connects, heap flat (was -40 B
// per cycle).
#include <zephyr/kernel.h>
#include <zephyr/spinlock.h>

// ETSTimer (esp_rom ets_sys.h); `arg` holds the esp_timer handle.
struct ets_timer {
  void* next;
  uint32_t expire, period;
  void* func;
  void* arg;
};
extern int esp_timer_stop(void* handle);
extern int esp_timer_delete(void* handle);
void __real_ets_timer_setfn(struct ets_timer* t, void* f, void* a);
void __real_ets_timer_done(struct ets_timer* t);

// The blob sets up about ten ETSTimers. A slot past this many is not guarded:
// it leaks as before rather than failing.
#define HELD 32
static struct {
  struct ets_timer* t;
  void* handle;
} held[HELD];
static struct k_spinlock lock;

static int find(struct ets_timer* t) {
  for (int i = 0; i < HELD; i++) {
    if (held[i].t == t) return i;
  }
  return -1;
}

void __wrap_ets_timer_setfn(struct ets_timer* t, void* f, void* a) {
  void* orphan = NULL;
  k_spinlock_key_t k = k_spin_lock(&lock);
  int i = find(t);
  if (i >= 0 && t->arg == NULL) {
    orphan = held[i].handle;
    held[i].handle = NULL;
  }
  k_spin_unlock(&lock, k);
  if (orphan) {
    esp_timer_stop(orphan);  // An armed timer cannot be deleted.
    esp_timer_delete(orphan);
  }
  __real_ets_timer_setfn(t, f, a);
  k = k_spin_lock(&lock);
  i = find(t);
  if (i < 0) i = find(NULL);
  if (i >= 0) {
    held[i].t = t;
    held[i].handle = t->arg;
  }
  k_spin_unlock(&lock, k);
}

void __wrap_ets_timer_done(struct ets_timer* t) {
  __real_ets_timer_done(t);
  k_spinlock_key_t k = k_spin_lock(&lock);
  int i = find(t);
  if (i >= 0) held[i].handle = NULL;
  k_spin_unlock(&lock, k);
}
