#include "obs.h"

#include <errno.h>
#include <string.h>

int obs_post(struct obs_table* t, const char* name, size_t len, int64_t value, int64_t now) {
  if (len == 0 || len > OBS_NAME_MAX || memchr(name, 0, len)) return -EINVAL;
  struct obs_cell* cell = NULL;
  for (int i = 0; i < OBS_CELLS; i++) {
    struct obs_cell* c = &t->cells[i];
    if (strlen(c->name) == len && memcmp(c->name, name, len) == 0) {
      cell = c;
      break;
    }
    if (cell == NULL && c->name[0] == 0) cell = c;
  }
  if (cell == NULL) return -ENOSPC;
  if (cell->name[0] == 0) {
    memcpy(cell->name, name, len);
    cell->name[len] = 0;
  }
  cell->value = value;
  cell->captured = now;
  cell->seq = 0;
  cell->unacked = true;
  return 0;
}

bool obs_next(struct obs_table* t, int64_t* seq, struct obs_cell* out) {
  struct obs_cell* next = NULL;
  for (int i = 0; i < OBS_CELLS; i++) {
    struct obs_cell* c = &t->cells[i];
    if (!c->unacked) continue;
    if (next == NULL || (c->seq != 0 && (next->seq == 0 || c->seq < next->seq))) next = c;
  }
  if (next == NULL) return false;
  if (next->seq == 0) next->seq = ++*seq;
  *out = *next;
  return true;
}

void obs_ack(struct obs_table* t, int64_t hw) {
  for (int i = 0; i < OBS_CELLS; i++) {
    struct obs_cell* c = &t->cells[i];
    if (c->unacked && c->seq != 0 && c->seq <= hw) {
      c->unacked = false;
      c->seq = 0;
    }
  }
}
