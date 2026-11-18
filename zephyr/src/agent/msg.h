// porta's check-in records (rete protocol/PROTOCOL-NSL.md §5.3; Go authority
// porta/internal/wire/message.go): the node's #pull and #push, and the
// gateway's #batch, #ack and #err replies. Plain C, host-tested (agent/test/).
#pragma once

#include <stdbool.h>
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

// A #push of one obs/apps/<name> cell row, an AppObs whose image is the
// installed blob and its lifecycle, "running" or "erased" (a run-once image after
// its erase) (MANIFEST.cddl `app-obs`; `image` is what lets porta compare goal and obs).
size_t msg_push_app(uint8_t* buf, size_t cap, const char* id, int64_t bid, int64_t mono, int64_t seq,
                    const char* name, const uint8_t hash[32], int64_t size, const char* lifecycle);

// A goal/apps/<name> value (MANIFEST.cddl `app`); pointers into the value.
struct msg_app {
  const uint8_t* sig;   // 64 B
  const uint8_t* hash;  // 32 B: the image's SHA-256
  int64_t size;         // -1: the BlobRef did not say
  bool keep;            // absent: true
};

// False unless value is an App with a 64 B sig and a 32 B #sha256 image hash.
// Fields the agent does not use (runlevel, triggers, arguments, lifecycle) are skipped.
bool msg_decode_app(const uint8_t* value, size_t len, struct msg_app* out);

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
