#include "console.h"

#include <stdio.h>
#include <string.h>

static bool find_last(void* arg, const uint8_t* entry, size_t len) {
  struct console_head head;
  if (len >= sizeof(head)) {
    memcpy(&head, entry, sizeof(head));
    *(uint32_t*)arg = head.line;
  }
  return true;
}

void console_start(struct console* c, const struct console_ring* ring, uint32_t bid, uint32_t acked) {
  memset(c, 0, sizeof(*c));
  c->bid = bid;
  c->acked = acked;
  uint32_t last = 0;
  if (ring) ring->walk(ring->ctx, find_last, &last);
  c->next = (last > acked ? last : acked) + 1;
  c->ring = ring;
}

// The length of the valid UTF-8 sequence at s (as Go's utf8.Valid, which porta
// decodes with: no overlongs, surrogates or code points over U+10FFFF), or 0.
static size_t utf8_at(const uint8_t* s, size_t n) {
  uint8_t b = s[0];
  if (b < 0x80) return 1;
  size_t len;
  uint8_t lo = 0x80, hi = 0xbf;  // the second byte's range
  if (b >= 0xc2 && b <= 0xdf) {
    len = 2;
  } else if (b >= 0xe0 && b <= 0xef) {
    len = 3;
    if (b == 0xe0) lo = 0xa0;
    if (b == 0xed) hi = 0x9f;
  } else if (b >= 0xf0 && b <= 0xf4) {
    len = 4;
    if (b == 0xf0) lo = 0x90;
    if (b == 0xf4) hi = 0x8f;
  } else {
    return 0;
  }
  if (n < len || s[1] < lo || s[1] > hi) return 0;
  for (size_t i = 2; i < len; i++) {
    if (s[i] < 0x80 || s[i] > 0xbf) return 0;
  }
  return len;
}

static void commit(struct console* c, int64_t now) {
  if (c->cut) memcpy(c->line + CONSOLE_LINE_MAX - (sizeof(CONSOLE_CUT) - 1), CONSOLE_CUT, sizeof(CONSOLE_CUT) - 1);
  uint8_t* text = (uint8_t*)c->line;
  for (size_t i = 0; i < c->len;) {
    size_t n = utf8_at(text + i, c->len - i);
    if (n == 0) {
      text[i] = '?';
      n = 1;
    }
    i += n;
  }
  uint8_t entry[CONSOLE_ENTRY_MAX];
  struct console_head head = {c->next++, c->bid, now};
  memcpy(entry, &head, sizeof(head));
  memcpy(entry + sizeof(head), c->line, c->len);
  c->ring->append(c->ring->ctx, entry, sizeof(head) + c->len);
  c->len = 0;
  c->cut = false;
}

void console_write(struct console* c, const uint8_t* bytes, size_t len, int64_t now) {
  if (c->ring == NULL) return;
  for (size_t i = 0; i < len; i++) {
    if (bytes[i] == '\n') {
      commit(c, now);
    } else if (c->len < CONSOLE_LINE_MAX) {
      c->line[c->len++] = bytes[i];
    } else {
      c->cut = true;
    }
  }
}

struct select {
  struct console_batch* out;
  uint32_t expected;  // the line number that follows the last row's
  size_t used;        // of out->text
};

// Adds a row of text (copied) standing for lines up to through; false if it
// does not fit.
static bool add(struct select* s, const char* text, size_t len, uint32_t through, const struct console_head* origin) {
  struct console_batch* out = s->out;
  if (out->n == CONSOLE_BATCH_ROWS || s->used + len > sizeof(out->text)) return false;
  memcpy(out->text + s->used, text, len);
  out->rows[out->n] = (struct msg_print){out->text + s->used, len, origin->bid, origin->mono};
  out->through[out->n] = through;
  out->n++;
  s->used += len;
  s->expected = through + 1;
  return true;
}

static bool select_entry(void* arg, const uint8_t* entry, size_t len) {
  struct select* s = arg;
  struct console_head head;
  if (len < sizeof(head)) return true;
  memcpy(&head, entry, sizeof(head));
  if (head.line < s->expected) return true;  // acked already
  if (head.line > s->expected) {
    char gap[sizeof("(4294967295 lines lost)")];
    int n = snprintf(gap, sizeof(gap), "(%u lines lost)", (unsigned)(head.line - s->expected));
    if (!add(s, gap, n, head.line - 1, &head)) return false;
  }
  return add(s, (const char*)entry + sizeof(head), len - sizeof(head), head.line, &head);
}

void console_select(const struct console* c, struct console_batch* out) {
  out->n = 0;
  if (c->ring == NULL) return;
  struct select s = {out, c->acked + 1, 0};
  c->ring->walk(c->ring->ctx, select_entry, &s);
}

void console_ack(struct console* c, uint32_t through) {
  if (through > c->acked) c->acked = through;
}
