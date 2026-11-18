// The staging partition an image is fetched into and verified in before main()
// installs it (nodus-toit design, addendum 2026-11-17). Plain C behind a flash
// seam, host-tested (agent/test/); the Zephyr side is store.c.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ed25519.h"

#ifdef __cplusplus
extern "C" {
#endif

// porta's Block2 size (porta coapsrv/blob.go maxSZX, PROTOCOL-NSL §5.3: "at ≤ 256 B").
#define STAGE_BLOCK 256
#define STAGE_HASH_SIZE 32

// The flash under the staging area. Each answers 0 or a negative errno.
struct stage_io {
  int (*read)(void* ctx, size_t offset, void* buf, size_t len);
  int (*write)(void* ctx, size_t offset, const void* buf, size_t len);
  int (*erase)(void* ctx);  // the whole area, to 0xff
  size_t size;
  void* ctx;
};

// Where a fetch resumes: the first all-0xff block, less one block, since a
// power cut can leave the last block written in part. Rewriting a block with
// the same bytes is harmless on NOR flash, and so is refetching a block of
// the image that happens to be all 0xff.
size_t stage_progress(const struct stage_io* io);

// SHA-256 of the first len staged bytes, read STAGE_BLOCK at a time.
int stage_hash(const struct stage_io* io, size_t len, uint8_t out[STAGE_HASH_SIZE]);

enum stage_verdict { STAGE_OK = 0, STAGE_IO, STAGE_BAD_HASH, STAGE_BAD_SIG };

// The staged bytes must hash to `hash`, and `sig` must be pub's ed25519
// signature of those 32 bytes (porta internal/blob/keys.go SignHash).
enum stage_verdict stage_verify(const struct stage_io* io, size_t len, const uint8_t hash[STAGE_HASH_SIZE],
                                const uint8_t sig[ED25519_SIG_SIZE], const uint8_t pub[ED25519_PUB_SIZE]);

// Parses porta's public key file's hex (64 characters). False if it is not one.
bool stage_parse_key(const char* hex, uint8_t pub[ED25519_PUB_SIZE]);

#ifdef __cplusplus
}
#endif
