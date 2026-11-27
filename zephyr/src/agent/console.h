// The remote console (nodus-toit design, addendum 2026-11-27): every line a
// Toit program prints is appended to a flash ring with a line number that
// never resets; at each check-in the agent pushes the lines after the last
// number porta acked, as tel/print rows. Plain C behind a ring seam,
// host-tested (agent/test/); the Zephyr side (FCB on log_partition) is
// store.c, and agent.c locks it, since the VM writes and the agent reads.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "msg.h"

#ifdef __cplusplus
extern "C" {
#endif

// A line's text in the ring, its cut mark included: a longer line keeps its
// start and ends in CONSOLE_CUT. A row's value must stay under porta's
// space.MaxValueBytes, and a row of this length must fit a push
// (console_test checks both).
#define CONSOLE_LINE_MAX 160
#define CONSOLE_CUT " [cut]"
// Bounds one push's rows. Not what limits a push: MSG_PUSH_MAX holds fewer
// rows than this even when every text is empty (console_test checks it).
#define CONSOLE_BATCH_ROWS 16

// An entry in the ring: this head, then the text (no NUL).
struct console_head {
  uint32_t line;  // never resets: continued from the ring's last entry at boot
  uint32_t bid;   // the boot it was printed in
  int64_t mono;   // that boot's uptime ms when its line ended
};
#define CONSOLE_ENTRY_MAX (sizeof(struct console_head) + CONSOLE_LINE_MAX)

// Answers true to be given the next entry.
typedef bool (*console_entry_fn)(void* arg, const uint8_t* entry, size_t len);

// The flash under the ring. Each answers 0 or a negative errno.
struct console_ring {
  // Appends one entry; when the ring is full it first drops its oldest entries.
  int (*append)(void* ctx, const void* entry, size_t len);
  // Calls fn with each entry, oldest first, until fn answers false.
  int (*walk)(void* ctx, console_entry_fn fn, void* arg);
  void* ctx;
};

struct console {
  const struct console_ring* ring;  // NULL: no ring, and writes are dropped
  uint32_t bid;
  uint32_t next;   // the number the next line takes
  uint32_t acked;  // the last line porta acked
  char line[CONSOLE_LINE_MAX];  // the line being written
  size_t len;
  bool cut;
};

// Starts c on ring (NULL: none) in boot bid, porta having acked lines up to
// `acked`. Numbering continues after the ring's last entry, or after acked if
// that is higher (the ring was erased).
void console_start(struct console* c, const struct console_ring* ring, uint32_t bid, uint32_t acked);

// Bytes the VM wrote to stdout or stderr at uptime now; each '\n' ends a line,
// which goes to the ring. A byte that is not part of valid UTF-8 is stored as
// '?': porta refuses a text that is not UTF-8. A line whose append fails has
// still taken its number, so it is counted as lost.
void console_write(struct console* c, const uint8_t* bytes, size_t len, int64_t now);

// The rows after the last acked line, oldest first, as many as rows and
// text hold. A jump in line numbers (lines the ring dropped before porta
// acked them) is one row "(N lines lost)", with the origin of the line after it.
struct console_batch {
  struct msg_print rows[CONSOLE_BATCH_ROWS];
  uint32_t through[CONSOLE_BATCH_ROWS];  // the last line number each row stands for
  size_t n;
  char text[MSG_PUSH_MAX];  // the rows' texts: a longer total cannot fit a push anyway
};
void console_select(const struct console* c, struct console_batch* out);

// porta acked every line up to through.
void console_ack(struct console* c, uint32_t through);

#ifdef __cplusplus
}
#endif
