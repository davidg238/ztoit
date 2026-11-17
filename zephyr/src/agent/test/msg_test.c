// Host test for the agent's codec: its bytes must equal what rete's Go cval
// encodes for the same records (goldens generated with protocol/cval, the
// codec porta decodes with), and it must decode cval's replies.
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

  printf(failures ? "FAILED %d\n" : "PASS\n", failures);
  return failures != 0;
}
