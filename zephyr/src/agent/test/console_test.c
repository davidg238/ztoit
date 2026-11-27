// Host test of the remote console (console.c) over a RAM ring that drops its
// oldest entry when full, as FCB drops its oldest sector: line assembly from
// partial writes, cut lines, UTF-8, numbering continued at a reboot, the rows
// after the last ack, and a gap as one "(N lines lost)" row. Also that a push
// is bounded by MSG_PUSH_MAX, not CONSOLE_BATCH_ROWS, and holds a longest line.
// Run: sh zephyr/src/agent/test/run.sh
#include <stdio.h>
#include <string.h>

#include "../console.h"

static int failures = 0;

static void check(const char* what, int cond) {
  printf("%s %s\n", cond ? "ok  " : "FAIL", what);
  if (!cond) failures++;
}

#define RING_ENTRIES 8

static struct {
  uint8_t entries[RING_ENTRIES][CONSOLE_ENTRY_MAX];
  size_t lens[RING_ENTRIES];
  size_t first, count;
  bool fail;  // the next append fails
} ram;

static int ram_append(void* ctx, const void* entry, size_t len) {
  (void)ctx;
  if (ram.fail) {
    ram.fail = false;
    return -5;
  }
  if (ram.count == RING_ENTRIES) {
    ram.first = (ram.first + 1) % RING_ENTRIES;
    ram.count--;
  }
  size_t i = (ram.first + ram.count++) % RING_ENTRIES;
  memcpy(ram.entries[i], entry, len);
  ram.lens[i] = len;
  return 0;
}

static int ram_walk(void* ctx, console_entry_fn fn, void* arg) {
  (void)ctx;
  for (size_t k = 0; k < ram.count; k++) {
    size_t i = (ram.first + k) % RING_ENTRIES;
    if (!fn(arg, ram.entries[i], ram.lens[i])) break;
  }
  return 0;
}

static const struct console_ring ring = {ram_append, ram_walk, NULL};

static void put(struct console* c, const char* s, int64_t now) {
  console_write(c, (const uint8_t*)s, strlen(s), now);
}

static bool row_is(const struct console_batch* b, size_t i, const char* text, uint32_t through, int64_t bid,
                   int64_t mono) {
  const struct msg_print* r = &b->rows[i];
  return i < b->n && r->len == strlen(text) && memcmp(r->text, text, r->len) == 0 && b->through[i] == through &&
         r->bid == bid && r->captured == mono;
}

static struct console_batch batch;

int main(void) {
  struct console c;

  console_start(&c, NULL, 1, 0);
  put(&c, "no ring\n", 1);
  console_select(&c, &batch);
  check("without a ring, writes are dropped and nothing is selected", batch.n == 0);

  console_start(&c, &ring, 1, 0);
  check("an empty ring numbers from 1", c.next == 1);
  console_select(&c, &batch);
  check("an empty ring selects nothing", batch.n == 0);

  put(&c, "net_cycle: ", 100);
  put(&c, "open 12 ms", 101);
  check("a partial line is not yet in the ring", ram.count == 0);
  put(&c, "\n", 102);
  put(&c, "a\nb\n", 200);
  console_select(&c, &batch);
  check("partial writes make one line; one write of two lines makes two",
        batch.n == 3 && row_is(&batch, 0, "net_cycle: open 12 ms", 1, 1, 102) && row_is(&batch, 1, "a", 2, 1, 200) &&
            row_is(&batch, 2, "b", 3, 1, 200));

  console_ack(&c, 2);
  console_select(&c, &batch);
  check("after an ack, only the lines after it", batch.n == 1 && row_is(&batch, 0, "b", 3, 1, 200));
  console_ack(&c, 3);
  console_select(&c, &batch);
  check("all acked: nothing", batch.n == 0);

  char longer[CONSOLE_LINE_MAX + 20];
  memset(longer, 'x', sizeof(longer));
  console_write(&c, (const uint8_t*)longer, sizeof(longer), 300);
  put(&c, "\n", 300);
  console_select(&c, &batch);
  check("a long line is cut to CONSOLE_LINE_MAX and ends in the mark",
        batch.n == 1 && batch.rows[0].len == CONSOLE_LINE_MAX &&
            memcmp(batch.rows[0].text + CONSOLE_LINE_MAX - strlen(CONSOLE_CUT), CONSOLE_CUT, strlen(CONSOLE_CUT)) == 0 &&
            batch.rows[0].text[0] == 'x');
  console_write(&c, (const uint8_t*)longer, CONSOLE_LINE_MAX, 301);
  put(&c, "\n", 301);
  console_select(&c, &batch);
  check("a line of exactly CONSOLE_LINE_MAX is not cut",
        batch.n == 2 && batch.rows[1].len == CONSOLE_LINE_MAX && batch.rows[1].text[CONSOLE_LINE_MAX - 1] == 'x');
  console_ack(&c, 5);

  put(&c, "caf\xc3\xa9 \xe2\x82\xac ok\n", 400);    // é, €: valid
  put(&c, "bad \xff \xc3 \xed\xa0\x80 end\n", 401);  // 0xff, a lone lead byte, a surrogate
  console_select(&c, &batch);
  check("valid UTF-8 is kept", row_is(&batch, 0, "caf\xc3\xa9 \xe2\x82\xac ok", 6, 1, 400));
  check("invalid UTF-8 bytes become '?'", row_is(&batch, 1, "bad ? ? ??? end", 7, 1, 401));
  console_ack(&c, 7);

  // A reboot: the ring keeps its entries, porta acked through 7.
  put(&c, "printed just before the reset\n", 500);
  console_start(&c, &ring, 2, 7);
  check("after a reboot, numbering continues from the ring's last entry", c.next == 9);
  put(&c, "first line of boot 2\n", 50);
  console_select(&c, &batch);
  check("the line from before the reboot comes with its own boot",
        batch.n == 2 && row_is(&batch, 0, "printed just before the reset", 8, 1, 500) &&
            row_is(&batch, 1, "first line of boot 2", 9, 2, 50));
  console_ack(&c, 9);

  // The ring was erased (a full flash erase) but porta's ack survived in settings.
  struct console c2;
  memset(&ram, 0, sizeof(ram));
  console_start(&c2, &ring, 3, 9);
  check("an erased ring continues after the ack, not from 1", c2.next == 10);

  // A gap: more lines than the ring holds, none acked.
  char text[32];
  for (int i = 0; i < RING_ENTRIES + 3; i++) {
    snprintf(text, sizeof(text), "line %d\n", i);
    put(&c2, text, 1000 + i);
  }
  console_select(&c2, &batch);
  check("lines the ring dropped unacked are one gap row first, with the next line's origin",
        batch.n == RING_ENTRIES + 1 && row_is(&batch, 0, "(3 lines lost)", 12, 3, 1003) &&
            row_is(&batch, 1, "line 3", 13, 3, 1003) && row_is(&batch, RING_ENTRIES, "line 10", 20, 3, 1010));
  console_ack(&c2, batch.through[0]);
  console_select(&c2, &batch);
  check("the gap acked, the lines after it remain", batch.n == RING_ENTRIES && row_is(&batch, 0, "line 3", 13, 3, 1003));
  console_ack(&c2, 20);

  ram.fail = true;
  put(&c2, "this append fails\n", 2000);
  put(&c2, "this one lands\n", 2001);
  console_select(&c2, &batch);
  check("a line whose append failed is counted lost", batch.n == 2 && row_is(&batch, 0, "(1 lines lost)", 21, 3, 2001) &&
                                                         row_is(&batch, 1, "this one lands", 22, 3, 2001));
  console_ack(&c2, 22);

  // Rows: the text buffer and the push.
  memset(&ram, 0, sizeof(ram));
  console_start(&c2, &ring, 3, 0);
  for (int i = 0; i < RING_ENTRIES; i++) {
    console_write(&c2, (const uint8_t*)longer, CONSOLE_LINE_MAX, 1);
    put(&c2, "\n", 1);
  }
  console_select(&c2, &batch);
  check("a batch takes what its text buffer holds", batch.n == MSG_PUSH_MAX / CONSOLE_LINE_MAX);

  uint8_t buf[MSG_PUSH_MAX];
  size_t n = 1;
  // Worst-case numbers: nine-byte seq, bid and monos.
  struct msg_print longest = {batch.rows[0].text, CONSOLE_LINE_MAX, INT64_MAX, INT64_MAX};
  check("a push of one row of CONSOLE_LINE_MAX fits MSG_PUSH_MAX",
        msg_push_prints(buf, sizeof(buf), "a4cf12345678abcd", INT64_MAX, INT64_MAX, INT64_MAX - 1, &longest, &n) &&
            n == 1);
  // porta's space.MaxValueBytes: a whole push of the row in that much holds its value too.
  uint8_t value[256];
  struct msg_print small = {batch.rows[0].text, CONSOLE_LINE_MAX, 1, 1};
  n = 1;
  check("its value is within porta's 256 B (space.MaxValueBytes)",
        msg_push_prints(value, sizeof(value), "a4cf12345678", 1, 1, 1, &small, &n) && n == 1);

  struct msg_print empty[CONSOLE_BATCH_ROWS];
  for (int i = 0; i < CONSOLE_BATCH_ROWS; i++) empty[i] = (struct msg_print){"", 0, 1, 0};
  n = CONSOLE_BATCH_ROWS;
  check("MSG_PUSH_MAX holds fewer than CONSOLE_BATCH_ROWS rows, even empty ones",
        msg_push_prints(buf, sizeof(buf), "a4cf12345678", 1, 0, 1, empty, &n) && n < CONSOLE_BATCH_ROWS);

  printf(failures ? "FAILED %d\n" : "PASS\n", failures);
  return failures != 0;
}
