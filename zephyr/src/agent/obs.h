// The cells apps post (nodus-toit design, addendum 2026-11-25): an int under a
// name, pushed by the agent as obs/<name> at the next check-in. The last post
// wins; nothing is queued behind it. Plain C, host-tested (agent/test/); the
// agent locks the table, since the VM posts and the agent pushes.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OBS_CELLS 4
// "obs/<name>" must fit msg_push_obs's path.
#define OBS_NAME_MAX 32

struct obs_cell {
  char name[OBS_NAME_MAX + 1];  // "": a free cell
  int64_t value;
  int64_t captured;             // uptime ms at the post (rete#444: a row's time is its capture)
  int64_t seq;                  // 0 until pushed; then resent with it until acked
  bool unacked;
};

struct obs_table {
  struct obs_cell cells[OBS_CELLS];
};

// Posts value under name at uptime now. 0; -EINVAL for a name that is empty,
// over OBS_NAME_MAX or holds a NUL; -ENOSPC when every cell holds another name.
// A post drops the cell's seq: a push of the older value may be in flight, and
// porta's ack of it must not ack this one.
int obs_post(struct obs_table* t, const char* name, size_t len, int64_t value, int64_t now);

// The unacked cell to push next, copied to *out; false if there is none. A cell
// that has a seq goes first, lowest first, since porta drops a seq at or under
// its high-water as a duplicate; then a cell without one, which takes ++*seq.
bool obs_next(struct obs_table* t, int64_t* seq, struct obs_cell* out);

// porta's high-water: every cell pushed with a seq at or under it is acked.
void obs_ack(struct obs_table* t, int64_t hw);

#ifdef __cplusplus
}
#endif
