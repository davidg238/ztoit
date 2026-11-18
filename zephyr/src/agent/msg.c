#include "msg.h"

#include <stdio.h>
#include <string.h>

#include <zcbor_decode.h>
#include <zcbor_encode.h>

// porta's CBOR (rete protocol/cval) on zcbor's low-level API. Two shapes a
// generic CBOR mapping would get wrong: a Symbol is tag 39 + text, and a Record
// is tag 27 + [Symbol name, Map] whose keys are Symbols. zcbor must be built
// ZCBOR_CANONICAL, or its lists come out indefinite-length, which cval refuses.
// Map keys are written in cval's order: shorter encoded key first, then
// bytewise (RFC 8949 §4.2); the goldens in agent/test/ pin both.

#ifndef ZCBOR_CANONICAL
#error "the agent's wire needs ZCBOR_CANONICAL (definite lengths)"
#endif

#define BACKUPS 8  // deepest nesting: push > ent > row > #AppObs > map > #BlobRef > map

static bool sym(zcbor_state_t* s, const char* name) {
  return zcbor_tag_put(s, 39) && zcbor_tstr_encode_ptr(s, name, strlen(name));
}

static bool str(zcbor_state_t* s, const char* text) {
  return zcbor_tstr_encode_ptr(s, text, strlen(text));
}

static bool record_start(zcbor_state_t* s, const char* name, size_t n_fields) {
  return zcbor_tag_put(s, 27) && zcbor_list_start_encode(s, 2) && sym(s, name) &&
         zcbor_map_start_encode(s, n_fields);
}

static bool record_end(zcbor_state_t* s, size_t n_fields) {
  return zcbor_map_end_encode(s, n_fields) && zcbor_list_end_encode(s, 2);
}

static size_t done(bool ok, zcbor_state_t* s, const uint8_t* buf) {
  return ok ? (size_t)(s->payload - buf) : 0;
}

size_t msg_pull(uint8_t* buf, size_t cap, const char* id, int64_t cursor, int64_t bid, int64_t hw,
                int64_t wak) {
  ZCBOR_STATE_E(s, BACKUPS, buf, cap, 1);
  bool ok = record_start(s, "pull", 5) &&
            sym(s, "hw") && zcbor_int64_put(s, hw) &&
            sym(s, "id") && str(s, id) &&
            sym(s, "bid") && zcbor_int64_put(s, bid) &&
            sym(s, "wak") && zcbor_int64_put(s, wak) &&
            sym(s, "cursor") && zcbor_int64_put(s, cursor) &&
            record_end(s, 5);
  return done(ok, s, buf);
}

// A #push of one row, [nodeSeq, path, kind, value, bid, monoMs]: push_open
// writes up to the value, the caller the value, push_close the rest.
static bool push_open(zcbor_state_t* s, const char* id, int64_t bid, int64_t seq, const char* path) {
  return record_start(s, "push", 4) &&
         sym(s, "id") && str(s, id) &&
         sym(s, "bid") && zcbor_int64_put(s, bid) &&
         sym(s, "ent") && zcbor_list_start_encode(s, 1) &&
           zcbor_list_start_encode(s, 6) &&
             zcbor_int64_put(s, seq) && str(s, path) && zcbor_int64_put(s, 2);  // cell
}

static bool push_close(zcbor_state_t* s, int64_t bid, int64_t mono) {
  return zcbor_int64_put(s, bid) && zcbor_int64_put(s, mono) &&
           zcbor_list_end_encode(s, 6) &&
         zcbor_list_end_encode(s, 1) &&
         sym(s, "mono") && zcbor_int64_put(s, mono) &&
         record_end(s, 4);
}

size_t msg_push_build(uint8_t* buf, size_t cap, const char* id, int64_t bid, int64_t mono,
                      int64_t seq, const struct msg_build* b) {
  ZCBOR_STATE_E(s, BACKUPS, buf, cap, 1);
  bool ok = push_open(s, id, bid, seq, "obs/build") &&
            record_start(s, "Build", 4) &&
              sym(s, "sdk") && str(s, b->sdk) &&
              sym(s, "chip") && sym(s, b->chip) &&
              sym(s, "board") && str(s, b->board) &&
              sym(s, "build") && str(s, b->build) &&
            record_end(s, 4) &&
            push_close(s, bid, mono);
  return done(ok, s, buf);
}

size_t msg_push_app(uint8_t* buf, size_t cap, const char* id, int64_t bid, int64_t mono, int64_t seq,
                    const char* name, const uint8_t hash[32], int64_t size, const char* lifecycle) {
  char path[64];
  int n = snprintf(path, sizeof(path), "obs/apps/%s", name);
  if (n < 0 || (size_t)n >= sizeof(path)) return 0;
  ZCBOR_STATE_E(s, BACKUPS, buf, cap, 1);
  bool ok = push_open(s, id, bid, seq, path) &&
            record_start(s, "AppObs", 2) &&
              sym(s, "image") &&
              record_start(s, "BlobRef", 2) &&
                sym(s, "hash") && zcbor_bstr_encode_ptr(s, (const char*)hash, 32) &&
                sym(s, "size") && zcbor_int64_put(s, size) &&
              record_end(s, 2) &&
              sym(s, "lifecycle") && sym(s, lifecycle) &&
            record_end(s, 2) &&
            push_close(s, bid, mono);
  return done(ok, s, buf);
}

static bool is(const struct zcbor_string* z, const char* want) {
  return z->len == strlen(want) && memcmp(z->value, want, z->len) == 0;
}

static bool read_sym(zcbor_state_t* s, struct zcbor_string* out) {
  return zcbor_tag_expect(s, 39) && zcbor_tstr_decode(s, out);
}

// A record's head: tag 27, [Symbol name, then its map opened.
static bool record_open(zcbor_state_t* s, const char* name) {
  struct zcbor_string got;
  return zcbor_tag_expect(s, 27) && zcbor_list_start_decode(s) && read_sym(s, &got) && is(&got, name) &&
         zcbor_map_start_decode(s);
}

static bool record_close(zcbor_state_t* s) {
  return zcbor_map_end_decode(s) && zcbor_list_end_decode(s);
}

// MANIFEST.cddl `blobref`: alg absent means #sha256 (RECORDS §2.3).
static bool decode_blobref(zcbor_state_t* s, struct msg_app* out) {
  if (!record_open(s, "BlobRef")) return false;
  while (!zcbor_array_at_end(s)) {
    struct zcbor_string key, text;
    if (!read_sym(s, &key)) return false;
    bool ok;
    if (is(&key, "alg")) {
      ok = read_sym(s, &text) && is(&text, "sha256");
    } else if (is(&key, "hash")) {
      ok = zcbor_bstr_decode(s, &text) && text.len == 32;
      out->hash = text.value;
    } else if (is(&key, "size")) {
      ok = zcbor_int64_decode(s, &out->size) && out->size >= 0;
    } else {
      ok = zcbor_any_skip(s, NULL);
    }
    if (!ok) return false;
  }
  return out->hash && record_close(s);
}

bool msg_decode_app(const uint8_t* value, size_t len, struct msg_app* out) {
  memset(out, 0, sizeof(*out));
  out->size = -1;
  out->keep = true;
  ZCBOR_STATE_D(s, BACKUPS, value, len, 1, 0);
  if (!record_open(s, "App")) return false;
  while (!zcbor_array_at_end(s)) {
    struct zcbor_string key, bytes;
    if (!read_sym(s, &key)) return false;
    bool ok;
    if (is(&key, "sig")) {
      ok = zcbor_bstr_decode(s, &bytes) && bytes.len == 64;
      out->sig = bytes.value;
    } else if (is(&key, "keep")) {
      ok = zcbor_bool_decode(s, &out->keep);
    } else if (is(&key, "image")) {
      ok = decode_blobref(s, out);
    } else {
      ok = zcbor_any_skip(s, NULL);
    }
    if (!ok) return false;
  }
  return out->sig && out->hash && record_close(s) && s->payload == value + len;
}

// #batch's ent: rows [seq, path, kind, value].
static bool decode_rows(zcbor_state_t* s, struct msg_reply* out, msg_entry_fn on_entry, void* ctx) {
  if (!zcbor_list_start_decode(s)) return false;
  while (!zcbor_array_at_end(s)) {
    int64_t seq, kind;
    struct zcbor_string path;
    if (!zcbor_list_start_decode(s) || !zcbor_int64_decode(s, &seq) || !zcbor_tstr_decode(s, &path) ||
        !zcbor_int64_decode(s, &kind)) {
      return false;
    }
    const uint8_t* value = s->payload;
    if (!zcbor_any_skip(s, NULL) || !zcbor_array_at_end(s) || !zcbor_list_end_decode(s)) return false;
    if (on_entry) on_entry(seq, (const char*)path.value, path.len, kind, value, s->payload - value, ctx);
    out->entries++;
  }
  return zcbor_list_end_decode(s);
}

void msg_decode_reply(const uint8_t* buf, size_t len, struct msg_reply* out, msg_entry_fn on_entry,
                      void* ctx) {
  memset(out, 0, sizeof(*out));
  ZCBOR_STATE_D(s, BACKUPS, buf, len, 1, 0);
  struct zcbor_string name;
  if (!zcbor_tag_expect(s, 27) || !zcbor_list_start_decode(s) || !read_sym(s, &name) ||
      !zcbor_map_start_decode(s)) {
    return;
  }
  enum msg_reply_kind kind = is(&name, "batch") ? MSG_BATCH
                             : is(&name, "ack") ? MSG_ACK
                             : is(&name, "err") ? MSG_ERR
                                                : MSG_BAD;
  if (kind == MSG_BAD) return;
  bool seen = false;
  while (!zcbor_array_at_end(s)) {
    struct zcbor_string key;
    bool ok;
    if (!read_sym(s, &key)) return;
    if (kind == MSG_BATCH && is(&key, "hi")) {
      ok = seen = zcbor_int64_decode(s, &out->hi);
    } else if (kind == MSG_BATCH && is(&key, "ent")) {
      ok = decode_rows(s, out, on_entry, ctx);
    } else if (kind == MSG_ACK && is(&key, "hw")) {
      ok = seen = zcbor_int64_decode(s, &out->hw);
    } else if (kind == MSG_ERR && is(&key, "msg")) {
      struct zcbor_string text;
      ok = seen = zcbor_tstr_decode(s, &text);
      out->text = (const char*)text.value;
      out->text_len = text.len;
    } else {
      ok = zcbor_any_skip(s, NULL);
    }
    if (!ok) return;
  }
  if (!seen || !zcbor_map_end_decode(s) || !zcbor_list_end_decode(s) || s->payload != buf + len) return;
  out->kind = kind;
}
