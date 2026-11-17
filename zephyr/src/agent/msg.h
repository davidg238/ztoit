// porta's check-in records (rete protocol/PROTOCOL-NSL.md §5.3; Go authority
// porta/internal/wire/message.go): the node's #pull and #push, and the
// gateway's #batch, #ack and #err replies. Plain C, host-tested (agent/test/).
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// obs/build's #Build (MANIFEST.cddl `build`). chip is a Symbol's name.
struct msg_build {
  const char* sdk;
  const char* chip;
  const char* board;
  const char* build;
};

// Each answers the encoded length, or 0 if it did not fit.
size_t msg_pull(uint8_t* buf, size_t cap, const char* id, int64_t cursor, int64_t bid, int64_t hw,
                int64_t wak);
// A #push of one obs/build cell row: node seq `seq`, captured at `mono` of boot `bid`.
size_t msg_push_build(uint8_t* buf, size_t cap, const char* id, int64_t bid, int64_t mono,
                      int64_t seq, const struct msg_build* b);

enum msg_reply_kind { MSG_BAD = 0, MSG_BATCH, MSG_ACK, MSG_ERR };

struct msg_reply {
  enum msg_reply_kind kind;
  int64_t hi;          // #batch
  int64_t hw;          // #ack
  const char* text;    // #err's msg, pointing into the reply buffer
  size_t text_len;
  int entries;         // #batch: rows handed to on_entry
};

// One #batch row [seq, path, kind, value]; value is its raw CBOR.
typedef void (*msg_entry_fn)(int64_t seq, const char* path, size_t path_len, int64_t kind,
                             const uint8_t* value, size_t value_len, void* ctx);

void msg_decode_reply(const uint8_t* buf, size_t len, struct msg_reply* out, msg_entry_fn on_entry,
                      void* ctx);

#ifdef __cplusplus
}
#endif
