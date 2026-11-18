// Host test of the staging logic (stage.c) on a RAM flash that behaves like
// NOR: erase sets 0xff, a write can only clear bits. The image, key and
// signature are test/gen/main.go's: Go's crypto/ed25519 over the SHA-256 of
// the 600-byte blob, as porta's SignHash does.
// Run: sh zephyr/src/agent/test/run.sh
#include <stdio.h>
#include <string.h>

#include "../stage.h"

static const char PUB[] = "03a107bff3ce10be1d70dd18e74bc09967e4d6309ba50d5f1ddc8664125531b8";
static const char HASH[] = "1783f1f6842889ff855d25b6d45d33dd7401ffa94eb93704f6a374c264cde486";
static const char SIG[] =
    "d5e06ad84a21acbc1a10ea63faf4277b384d8157a38b4ffc3f1319db87011ceb"
    "3f08157402bc0288d1f8506a5b73fae4c2ad70097c085edd3cf8dd678e34bf01";
#define BLOB_SIZE 600  // three blocks: 256 + 256 + 88

static uint8_t flash[8 * STAGE_BLOCK];

static int ram_read(void* ctx, size_t offset, void* buf, size_t len) {
  (void)ctx;
  memcpy(buf, flash + offset, len);
  return 0;
}

static int ram_write(void* ctx, size_t offset, const void* buf, size_t len) {
  (void)ctx;
  for (size_t i = 0; i < len; i++) flash[offset + i] &= ((const uint8_t*)buf)[i];
  return 0;
}

static int ram_erase(void* ctx) {
  (void)ctx;
  memset(flash, 0xff, sizeof(flash));
  return 0;
}

static const struct stage_io io = {ram_read, ram_write, ram_erase, sizeof(flash), NULL};

static int failures = 0;

static void check(const char* what, int cond) {
  printf("%s %s\n", cond ? "ok  " : "FAIL", what);
  if (!cond) failures++;
}

static void unhex(const char* s, uint8_t* out) {
  for (size_t i = 0; i < strlen(s) / 2; i++) sscanf(s + 2 * i, "%2hhx", &out[i]);
}

int main(void) {
  uint8_t blob[BLOB_SIZE], pub[32], hash[32], sig[64];
  for (int i = 0; i < BLOB_SIZE; i++) blob[i] = (uint8_t)(i * 7 + 3);  // as gen/main.go
  unhex(PUB, pub);
  unhex(HASH, hash);
  unhex(SIG, sig);

  io.erase(io.ctx);
  check("erased staging: progress 0", stage_progress(&io) == 0);

  // A fetch cut by a power loss part-way through block 1.
  io.write(io.ctx, 0, blob, STAGE_BLOCK);
  io.write(io.ctx, STAGE_BLOCK, blob + STAGE_BLOCK, 100);
  check("cut in block 1: resumes at block 1", stage_progress(&io) == STAGE_BLOCK);
  check("partial staging does not verify", stage_verify(&io, BLOB_SIZE, hash, sig, pub) == STAGE_BAD_HASH);
  // Resumed: block 1 rewritten whole over its partial bytes, then block 2.
  io.write(io.ctx, STAGE_BLOCK, blob + STAGE_BLOCK, STAGE_BLOCK);
  io.write(io.ctx, 2 * STAGE_BLOCK, blob + 2 * STAGE_BLOCK, BLOB_SIZE - 2 * STAGE_BLOCK);
  check("all staged: resumes at the last block", stage_progress(&io) == 2 * STAGE_BLOCK);

  uint8_t got[32];
  check("hash of the staged bytes is porta's", stage_hash(&io, BLOB_SIZE, got) == 0 && !memcmp(got, hash, 32));
  check("porta's signature verifies", stage_verify(&io, BLOB_SIZE, hash, sig, pub) == STAGE_OK);

  sig[17] ^= 0x01;
  check("a flipped signature bit is refused", stage_verify(&io, BLOB_SIZE, hash, sig, pub) == STAGE_BAD_SIG);
  sig[17] ^= 0x01;
  pub[0] ^= 0x01;
  check("another key is refused", stage_verify(&io, BLOB_SIZE, hash, sig, pub) == STAGE_BAD_SIG);
  pub[0] ^= 0x01;
  uint8_t zero = 0;
  io.write(io.ctx, 300, &zero, 1);  // a corrupted staged byte
  check("a corrupted image is refused by hash", stage_verify(&io, BLOB_SIZE, hash, sig, pub) == STAGE_BAD_HASH);
  check("a length past the area is an IO error", stage_verify(&io, sizeof(flash) + 1, hash, sig, pub) == STAGE_IO);

  uint8_t key[32];
  check("porta's pub file parses", stage_parse_key(PUB, key) && !memcmp(key, pub, 32));
  check("empty key is refused", !stage_parse_key("", key));
  check("short key is refused", !stage_parse_key("03a107", key));
  char bad[65];
  memcpy(bad, PUB, sizeof(bad));
  bad[10] = 'g';
  check("non-hex key is refused", !stage_parse_key(bad, key));

  printf(failures ? "FAILED %d\n" : "PASS\n", failures);
  return failures != 0;
}
