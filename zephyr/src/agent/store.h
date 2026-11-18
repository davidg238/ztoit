// The agent's install state on Zephyr (nodus-toit design, addendum 2026-11-17):
// the staging partition behind stage.h's seam, and two kinds of settings record.
//
//   agent/stage       the image in staging: fetching, verified, or refused.
//   agent/app/<name>  an image main() installed in the registry, by name.
//
// Two kinds, not one record per name moving through states, so a refused or
// half-fetched new image never overwrites what is installed under that name.
// Shared by agent.c and main.cc; main() writes the registry only before the
// VM and the agent thread start.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "stage.h"

#ifdef __cplusplus
extern "C" {
#endif

// An app's name is the last segment of goal/apps/<name>. Longer names are
// refused: "agent/app/<name>" must fit a settings name (SETTINGS_MAX_NAME_LEN).
#define STORE_NAME_MAX 32

enum store_stage_state { STAGE_FETCHING = 1, STAGE_VERIFIED, STAGE_REFUSED };

struct store_stage {
  char name[STORE_NAME_MAX + 1];
  uint8_t hash[STAGE_HASH_SIZE];
  uint8_t sig[ED25519_SIG_SIZE];
  int64_t size;  // -1 until the last block arrives, if the BlobRef did not say
  uint8_t keep;
  uint8_t state;  // store_stage_state
};

enum store_app_state {
  APP_INSTALLED = 1,  // in the registry at offset
  APP_RAN,            // keep false, ran at this boot: erased at the next
  APP_ERASED,         // keep false and erased; kept so the same goal does not install again
};

struct store_app {
  uint8_t hash[STAGE_HASH_SIZE];
  int64_t size;
  int32_t offset;  // in the registry
  uint8_t id[16];  // the program's id (FlashAllocation::Header::ID_SIZE): is offset still ours?
  uint8_t keep;
  uint8_t state;     // store_app_state
  uint8_t reported;  // obs/apps/<name> acked by porta
};

// Settings up; false (and said on the console) if not.
bool store_init(void);

// The staging partition, or NULL on a board without one.
const struct stage_io* store_staging(void);

bool store_stage_load(struct store_stage* out);
int store_stage_save(const struct store_stage* stage);
int store_stage_clear(void);

bool store_app_load(const char* name, struct store_app* out);
int store_app_save(const char* name, const struct store_app* app);
// Marks agent/app/<name> unreported, so the agent pushes obs/apps/<name> again
// (a refused new image under that name: #38). No record: nothing to say.
void store_app_unreport(const char* name);

// Calls fn for each agent/app/<name> record.
typedef void (*store_app_fn)(const char* name, struct store_app* app, void* ctx);
void store_apps_each(store_app_fn fn, void* ctx);
// Deletes every agent/app/<name> record (the registry was erased).
void store_apps_clear(void);

#ifdef __cplusplus
}
#endif
