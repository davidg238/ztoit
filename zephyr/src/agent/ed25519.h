// ed25519 verify-only (vendored TweetNaCl, see ed25519.c).
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ED25519_SIG_SIZE 64
#define ED25519_PUB_SIZE 32
// porta signs the 32 raw bytes of the image's SHA-256 (porta internal/blob/keys.go SignHash).
#define ED25519_MSG_SIZE 32

// Answers 1 if sig is pub's signature of msg, else 0.
int ed25519_verify(const uint8_t sig[ED25519_SIG_SIZE], const uint8_t msg[ED25519_MSG_SIZE],
                   const uint8_t pub[ED25519_PUB_SIZE]);

#ifdef __cplusplus
}
#endif
