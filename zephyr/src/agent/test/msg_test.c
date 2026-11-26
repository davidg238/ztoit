// Host test for the agent's codec: its bytes must equal what rete's Go cval
// encodes for the same records (goldens generated with protocol/cval, the
// codec porta decodes with), and it must decode cval's replies.
// The install goldens (GOLD_app*, GOLD_push_app) and GOLD_push_obs come from porta's own
// encoders: test/gen/main.go, `go run .` there.
// Run: sh zephyr/src/agent/test/run.sh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../msg.h"

static const char GOLD_pull[] = "d81b82d8276470756c6ca5d82762687700d8276269646c613463663132333435363738d8276362696407d8276377616b01d82766637572736f7219012c";
static const char GOLD_push[] = "d81b82d8276470757368a4d8276269646c613463663132333435363738d8276362696407d82763656e74818601696f62732f6275696c6402d81b82d827654275696c64a4d8276373646b6676322e302e30d8276463686970d827656573703332d82765626f6172646d65737033325f6465766b697463d827656275696c646d7a746f69742034343336633262071a00011170d827646d6f6e6f1a00011170";
static const char GOLD_batch[] = "d81b82d827656261746368a2d8276268690cd82763656e7482840b69676f616c2f6e616d65026562656e6368840c69676f616c2f6d6f646502d81b82d827644d6f6465a1d827646d6f6465d827626f6e";
static const char GOLD_ack[] = "d81b82d8276361636ba1d82762687701";
static const char GOLD_err[] = "d81b82d82763657272a1d827636d7367646e6f7065";

// porta's appRecord (portacli/blobcmd.go): image {hash, size 600} and sig.
static const char GOLD_app[] = "d81b82d82763417070a2d827637369675840d5e06ad84a21acbc1a10ea63faf4277b384d8157a38b4ffc3f1319db87011ceb3f08157402bc0288d1f8506a5b73fae4c2ad70097c085edd3cf8dd678e34bf01d82765696d616765d81b82d82767426c6f62526566a2d827646861736858201783f1f6842889ff855d25b6d45d33dd7401ffa94eb93704f6a374c264cde486d8276473697a65190258";
// Every optional field of MANIFEST.cddl `app`, keep false, alg #sha256, no size.
static const char GOLD_app_full[] = "d81b82d82763417070a7d827637369675840d5e06ad84a21acbc1a10ea63faf4277b384d8157a38b4ffc3f1319db87011ceb3f08157402bc0288d1f8506a5b73fae4c2ad70097c085edd3cf8dd678e34bf01d827646b656570f4d82765696d616765d81b82d82767426c6f62526566a2d82763616c67d82766736861323536d827646861736858201783f1f6842889ff855d25b6d45d33dd7401ffa94eb93704f6a374c264cde486d8276872756e6c6576656c03d82768747269676765727381d82764626f6f74d82769617267756d656e7473a1d827616e01d827696c6966656379636c65d8276772756e4c6f6f70";
// alg #blake3: not a hash the agent checks.
static const char GOLD_app_alg[] = "d81b82d82763417070a2d827637369675840d5e06ad84a21acbc1a10ea63faf4277b384d8157a38b4ffc3f1319db87011ceb3f08157402bc0288d1f8506a5b73fae4c2ad70097c085edd3cf8dd678e34bf01d82765696d616765d81b82d82767426c6f62526566a2d82763616c67d82766626c616b6533d827646861736858201783f1f6842889ff855d25b6d45d33dd7401ffa94eb93704f6a374c264cde486";
// wire.EncodePush of obs/apps/hello = AppObs{image: the same BlobRef}, seq 2.
static const char GOLD_push_app[] = "d81b82d8276470757368a4d8276269646c613463663132333435363738d8276362696407d82763656e748186026e6f62732f617070732f68656c6c6f02d81b82d827664170704f6273a2d82765696d616765d81b82d82767426c6f62526566a2d827646861736858201783f1f6842889ff855d25b6d45d33dd7401ffa94eb93704f6a374c264cde486d8276473697a65190258d827696c6966656379636c65d8276772756e6e696e67071a00011170d827646d6f6e6f1a00011170";
static const char GOLD_push_app_erased[] = "d81b82d8276470757368a4d8276269646c613463663132333435363738d8276362696407d82763656e748186026e6f62732f617070732f68656c6c6f02d81b82d827664170704f6273a2d82765696d616765d81b82d82767426c6f62526566a2d827646861736858201783f1f6842889ff855d25b6d45d33dd7401ffa94eb93704f6a374c264cde486d8276473697a65190258d827696c6966656379636c65d82766657261736564071a00011170d827646d6f6e6f1a00011170";
static const char GOLD_push_app_refused[] = "d81b82d8276470757368a4d8276269646c613463663132333435363738d8276362696407d82763656e748186026e6f62732f617070732f68656c6c6f02d81b82d827664170704f6273a2d82765696d616765d81b82d82767426c6f62526566a2d827646861736858201783f1f6842889ff855d25b6d45d33dd7401ffa94eb93704f6a374c264cde486d8276473697a65190258d827696c6966656379636c65d8276772656675736564071a00011170d827646d6f6e6f1a00011170";
// wire.EncodePush of obs/vin/pm25 = 37, seq 3: the row captured at 65000, the message sent at 70000.
static const char GOLD_push_obs[] = "d81b82d8276470757368a4d8276269646c613463663132333435363738d8276362696407d82763656e748186036c6f62732f76696e2f706d32350218250719fde8d827646d6f6e6f1a00011170";
static const char HASH[] = "1783f1f6842889ff855d25b6d45d33dd7401ffa94eb93704f6a374c264cde486";
static const char SIG[] = "d5e06ad84a21acbc1a10ea63faf4277b384d8157a38b4ffc3f1319db87011ceb3f08157402bc0288d1f8506a5b73fae4c2ad70097c085edd3cf8dd678e34bf01";

static int failures = 0;

static size_t unhex(const char* s, uint8_t* out) {
  size_t n = strlen(s) / 2;
  for (size_t i = 0; i < n; i++) sscanf(s + 2 * i, "%2hhx", &out[i]);
  return n;
}

static void same(const char* what, const uint8_t* got, size_t len, const char* want) {
  char hex[1024];
  for (size_t i = 0; i < len; i++) sprintf(hex + 2 * i, "%02x", got[i]);
  hex[2 * len] = 0;
  if (strcmp(hex, want) != 0) {
    printf("FAIL %s\n  got  %s\n  want %s\n", what, hex, want);
    failures++;
  } else {
    printf("ok   %s\n", what);
  }
}

static void check(const char* what, int cond) {
  printf("%s %s\n", cond ? "ok  " : "FAIL", what);
  if (!cond) failures++;
}

static int64_t seqs[4];
static char paths[4][32];
static size_t value_lens[4];
static void on_entry(int64_t seq, const char* path, size_t path_len, int64_t kind, const uint8_t* value,
                     size_t value_len, void* ctx) {
  (void)kind;
  (void)value;
  int* n = ctx;
  seqs[*n] = seq;
  memcpy(paths[*n], path, path_len);
  paths[*n][path_len] = 0;
  value_lens[*n] = value_len;
  (*n)++;
}

int main(void) {
  uint8_t buf[512], in[512];
  size_t n = msg_pull(buf, sizeof buf, "a4cf12345678", 300, 7, 0, 1);
  same("pull encodes as cval", buf, n, GOLD_pull);

  struct msg_build b = {"v2.0.0", "esp32", "esp32_devkitc", "ztoit 4436c2b"};
  n = msg_push_build(buf, sizeof buf, "a4cf12345678", 7, 70000, 1, &b);
  same("push(obs/build) encodes as cval", buf, n, GOLD_push);
  check("push into a short buffer answers 0", msg_push_build(buf, 20, "a4cf12345678", 7, 70000, 1, &b) == 0);

  struct msg_reply r;
  int count = 0;
  n = unhex(GOLD_batch, in);
  msg_decode_reply(in, n, &r, on_entry, &count);
  check("batch decodes", r.kind == MSG_BATCH && r.hi == 12 && r.entries == 2 && count == 2);
  check("batch rows", seqs[0] == 11 && !strcmp(paths[0], "goal/name") && value_lens[0] == 6 &&
                          seqs[1] == 12 && !strcmp(paths[1], "goal/mode"));
  msg_decode_reply(in, n - 1, &r, NULL, NULL);
  check("truncated batch is BAD", r.kind == MSG_BAD);

  n = unhex(GOLD_ack, in);
  msg_decode_reply(in, n, &r, NULL, NULL);
  check("ack decodes", r.kind == MSG_ACK && r.hw == 1);

  n = unhex(GOLD_err, in);
  msg_decode_reply(in, n, &r, NULL, NULL);
  check("err decodes", r.kind == MSG_ERR && r.text_len == 4 && !memcmp(r.text, "nope", 4));

  uint8_t hash[32], sig[64];
  unhex(HASH, hash);
  unhex(SIG, sig);
  n = msg_push_app(buf, sizeof buf, "a4cf12345678", 7, 70000, 2, "hello", hash, 600, "running");
  same("push(obs/apps/hello, #running) encodes as wire.EncodePush", buf, n, GOLD_push_app);
  n = msg_push_app(buf, sizeof buf, "a4cf12345678", 7, 70000, 2, "hello", hash, 600, "erased");
  same("push(obs/apps/hello, #erased) encodes as wire.EncodePush", buf, n, GOLD_push_app_erased);
  n = msg_push_app(buf, sizeof buf, "a4cf12345678", 7, 70000, 2, "hello", hash, 600, "refused");
  same("push(obs/apps/hello, #refused) encodes as wire.EncodePush", buf, n, GOLD_push_app_refused);

  n = msg_push_obs(buf, sizeof buf, "a4cf12345678", 7, 70000, 3, "vin/pm25", 37, 65000);
  same("push(obs/vin/pm25 = 37) encodes as wire.EncodePush", buf, n, GOLD_push_obs);
  check("push(obs/<name>) into a short buffer answers 0",
        msg_push_obs(buf, 40, "a4cf12345678", 7, 70000, 3, "vin/pm25", 37, 65000) == 0);

  struct msg_app a;
  n = unhex(GOLD_app, in);
  check("porta's App decodes", msg_decode_app(in, n, &a) && !memcmp(a.hash, hash, 32) && !memcmp(a.sig, sig, 64) &&
                                   a.size == 600 && a.keep);
  check("truncated App is refused", !msg_decode_app(in, n - 1, &a));
  n = unhex(GOLD_app_full, in);
  check("App with every field decodes, keep false", msg_decode_app(in, n, &a) && !memcmp(a.hash, hash, 32) &&
                                                        a.size == -1 && !a.keep);
  n = unhex(GOLD_app_alg, in);
  check("App with alg #blake3 is refused", !msg_decode_app(in, n, &a));
  n = unhex(GOLD_push_app, in);
  check("a #push is not an App", !msg_decode_app(in, n, &a));

  printf(failures ? "FAILED %d\n" : "PASS\n", failures);
  return failures != 0;
}
