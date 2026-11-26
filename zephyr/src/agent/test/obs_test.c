// Host test of the app observation table (obs.c): last post wins, a cell keeps
// its seq until acked, and a post while its push is in flight takes a new seq.
// Run: sh zephyr/src/agent/test/run.sh
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "../obs.h"

static int failures = 0;

static void check(const char* what, int cond) {
  printf("%s %s\n", cond ? "ok  " : "FAIL", what);
  if (!cond) failures++;
}

static int post(struct obs_table* t, const char* name, int64_t value, int64_t now) {
  return obs_post(t, name, strlen(name), value, now);
}

int main(void) {
  struct obs_table t;
  struct obs_cell c;
  int64_t seq = 1;  // as the agent's node_seq: obs/build holds 1
  memset(&t, 0, sizeof(t));

  check("an empty table has nothing to push", !obs_next(&t, &seq, &c));

  check("a post is taken", post(&t, "vin/pm25", 30, 100) == 0);
  check("it is pushed with the next seq, its value and its time",
        obs_next(&t, &seq, &c) && !strcmp(c.name, "vin/pm25") && c.value == 30 && c.captured == 100 &&
            c.seq == 2 && seq == 2);
  check("unacked, it is resent with the same seq", obs_next(&t, &seq, &c) && c.seq == 2 && seq == 2);
  obs_ack(&t, 2);
  check("acked, nothing is left to push", !obs_next(&t, &seq, &c));

  post(&t, "vin/pm25", 31, 200);
  post(&t, "vin/pm25", 32, 300);
  check("the last post wins, with a new seq", obs_next(&t, &seq, &c) && c.value == 32 && c.captured == 300 &&
                                                  c.seq == 3);
  obs_ack(&t, 3);

  // In flight: pushed with seq 4, posted again before porta's ack of 4 arrives.
  post(&t, "vin/pm25", 40, 400);
  check("pushed as seq 4", obs_next(&t, &seq, &c) && c.value == 40 && c.seq == 4);
  post(&t, "vin/pm25", 41, 410);
  obs_ack(&t, 4);
  check("a post while its push is in flight is not acked by that push's ack",
        obs_next(&t, &seq, &c) && c.value == 41 && c.captured == 410);
  check("it takes a new seq", c.seq == 5);
  obs_ack(&t, 5);

  // A cell resent with its seq goes before one that takes a new seq: porta
  // drops a seq at or under its high-water as a duplicate.
  post(&t, "a", 1, 500);  // the first free cell after vin/pm25
  check("a new cell takes seq 6", obs_next(&t, &seq, &c) && !strcmp(c.name, "a") && c.seq == 6);
  post(&t, "vin/pm25", 50, 510);  // the lower cell, no seq yet
  check("the unacked seq 6 goes first", obs_next(&t, &seq, &c) && !strcmp(c.name, "a") && c.seq == 6);
  obs_ack(&t, 6);
  check("then the new post, seq 7", obs_next(&t, &seq, &c) && !strcmp(c.name, "vin/pm25") && c.seq == 7);
  obs_ack(&t, 7);

  check("a third name fits", post(&t, "b", 1, 600) == 0);
  check("a fourth name fits", post(&t, "c", 1, 600) == 0);
  check("a fifth name is refused: the table is full", post(&t, "d", 1, 600) == -ENOSPC);
  check("a name already there still posts", post(&t, "b", 2, 600) == 0);

  char name[OBS_NAME_MAX + 2];
  memset(name, 'n', sizeof(name));
  name[OBS_NAME_MAX + 1] = 0;
  check("a name over OBS_NAME_MAX is refused", post(&t, name, 1, 700) == -EINVAL);
  check("an empty name is refused", post(&t, "", 1, 700) == -EINVAL);
  check("a name holding a NUL is refused", obs_post(&t, "vin\0x", 5, 1, 700) == -EINVAL);
  memset(&t, 0, sizeof(t));
  name[OBS_NAME_MAX] = 0;
  check("a name of OBS_NAME_MAX posts", post(&t, name, 1, 700) == 0 && obs_next(&t, &seq, &c) &&
                                            strlen(c.name) == OBS_NAME_MAX);

  printf(failures ? "FAILED %d\n" : "PASS\n", failures);
  return failures != 0;
}
