#include "stage.h"

#include <string.h>

#include <mbedtls/sha256.h>

static bool erased(const uint8_t* p, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (p[i] != 0xff) return false;
  }
  return true;
}

size_t stage_progress(const struct stage_io* io) {
  uint8_t block[STAGE_BLOCK];
  size_t offset = 0;
  while (offset + STAGE_BLOCK <= io->size) {
    if (io->read(io->ctx, offset, block, STAGE_BLOCK) != 0 || erased(block, STAGE_BLOCK)) break;
    offset += STAGE_BLOCK;
  }
  return offset == 0 ? 0 : offset - STAGE_BLOCK;
}

int stage_hash(const struct stage_io* io, size_t len, uint8_t out[STAGE_HASH_SIZE]) {
  if (len > io->size) return -1;
  uint8_t block[STAGE_BLOCK];
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  int err = mbedtls_sha256_starts(&sha, 0);
  for (size_t offset = 0; err == 0 && offset < len; offset += STAGE_BLOCK) {
    size_t n = len - offset < STAGE_BLOCK ? len - offset : STAGE_BLOCK;
    err = io->read(io->ctx, offset, block, n);
    if (err == 0) err = mbedtls_sha256_update(&sha, block, n);
  }
  if (err == 0) err = mbedtls_sha256_finish(&sha, out);
  mbedtls_sha256_free(&sha);
  return err;
}

enum stage_verdict stage_verify(const struct stage_io* io, size_t len, const uint8_t hash[STAGE_HASH_SIZE],
                                const uint8_t sig[ED25519_SIG_SIZE], const uint8_t pub[ED25519_PUB_SIZE]) {
  uint8_t got[STAGE_HASH_SIZE];
  if (stage_hash(io, len, got) != 0) return STAGE_IO;
  if (memcmp(got, hash, STAGE_HASH_SIZE) != 0) return STAGE_BAD_HASH;
  return ed25519_verify(sig, hash, pub) ? STAGE_OK : STAGE_BAD_SIG;
}

static int nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool stage_parse_key(const char* hex, uint8_t pub[ED25519_PUB_SIZE]) {
  if (strlen(hex) != 2 * ED25519_PUB_SIZE) return false;
  for (int i = 0; i < ED25519_PUB_SIZE; i++) {
    int hi = nibble(hex[2 * i]), lo = nibble(hex[2 * i + 1]);
    if (hi < 0 || lo < 0) return false;
    pub[i] = (uint8_t)(hi << 4 | lo);
  }
  return true;
}
