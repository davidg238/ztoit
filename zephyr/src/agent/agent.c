// The C agent's check-in (nodus-toit design §5, rete #521): first slice.
//
// Piggyback (design §7): each time the link comes up for a net.open (WiFi: the
// IPv4 lease; Thread: the fork's thread_up calls ztoit_link_up), the agent
// checks in with porta over CoAP:
//   POST /space #pull  -> #batch (logged; the cursor advances)
//   POST /space #push  -> #ack   (obs/build, once per boot, until acked;
//                                  obs/apps/<name> after an install, until acked;
//                                  obs/<name> after an app posts it, until acked;
//                                  tel/print, the lines printed since the last ack)
//   GET /blob/<hash>   -> 2.05   (an image, a Block2 block at a time)
// Install (design addendum 2026-11-17): a goal/apps/<name> row whose image is
// not the installed one is fetched into staging across check-ins, verified,
// and the node reboots; main() installs it into the registry before the VM.
// Observations (design addendum 2026-11-25): an app posts an int under a name
// (zephyr.observe, ztoit_observe) into obs.c's table, and each check-in pushes
// every cell not yet acked. Posting does not open the network.
// Console (design addendum 2026-11-27): every line the VM prints goes to a
// flash ring (ztoit_console, console.c), and each check-in pushes the lines
// after the last one porta acked.
// Not yet: other goals, safe mode.
//
// State: `bid` is a boot counter persisted with Zephyr settings (rete
// PROTOCOL-NSL §5.3: porta tells a reboot from a lost ack by it); cursor, hw
// and wak are RAM, as on an nsl node, and resync from 0 after a reboot. The
// install state and the console's last acked line are store.h's records.
#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/net/coap_client.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/reboot.h>
#ifdef CONFIG_WIFI_ESP32
#include <esp_wifi.h>
#endif
#if defined(CONFIG_NET_L2_OPENTHREAD)
#include <openthread/thread.h>
#include <zephyr/net/openthread.h>
#endif

#include "console.h"
#include "msg.h"
#include "obs.h"
#include "store.h"
#include "ztoit_ids.h"  // ZTOIT_SDK, ZTOIT_BUILD: generated each build (zephyr/ztoit_ids.cmake)

#define REPLY_MAX CONFIG_COAP_CLIENT_MESSAGE_SIZE
#define TIMEOUT_MS 15000

static struct coap_client client;
static K_SEM_DEFINE(addr_sem, 0, 1);
static K_SEM_DEFINE(reply_sem, 0, 1);
// A check-in is owed from the moment the link comes up until it ends, so a
// link taken down in between waits for it (ztoit_link_going_down).
static atomic_t owed;
static K_SEM_DEFINE(settled_sem, 0, 1);

static char id[2 * 8 + 1];
static int64_t bid;
static int64_t cursor, hw, wak;
static bool build_acked;
// Node seqs restart at 1 each boot (porta rewinds its high-water on a new
// bid). obs/build is seq 1; each obs/apps and obs/<name> push takes the next.
static const int64_t BUILD_SEQ = 1;
static int64_t node_seq = BUILD_SEQ;

static uint8_t request[MSG_PUSH_MAX];
static uint8_t reply[REPLY_MAX];
static size_t reply_len;
static int16_t reply_code;

// --- Boot counter -------------------------------------------------------------

static int bid_loaded(const char* key, size_t len, settings_read_cb read, void* cb_arg, void* param) {
  if (len == sizeof(bid)) read(cb_arg, &bid, sizeof(bid));
  return 0;
}

static void count_boot(void) {
  int err = store_init() ? 0 : -EIO;
  if (err == 0) err = settings_load_subtree_direct("agent/bid", bid_loaded, NULL);
  bid++;
  if (err == 0) err = settings_save_one("agent/bid", &bid, sizeof(bid));
  if (err != 0) {
    // 0 is porta's "no persistent boot identity": said, not hidden (porta logs it too).
    printk("agent: boot counter unavailable (%d): bid 0\n", err);
    bid = 0;
  }
}

// --- CoAP exchange --------------------------------------------------------------

static void on_reply(const struct coap_client_response_data* data, void* user_data) {
  int16_t code = data->result_code;
  size_t offset = data->offset;
  const uint8_t* payload = data->payload;
  size_t len = data->payload_len;
  bool last = data->last_block;
  reply_code = code;
  reply_len = 0;
  if (code >= 0 && payload && offset == 0 && len <= sizeof(reply)) {
    memcpy(reply, payload, len);
    reply_len = len;
  }
  if (last || code < 0) k_sem_give(&reply_sem);
}

static bool exchange(int sock, const struct sockaddr* porta, size_t len, struct msg_reply* out,
                     msg_entry_fn on_entry) {
  struct coap_client_request req = {
      .method = COAP_METHOD_POST,
      .confirmable = true,
      .path = "space",
      .fmt = COAP_CONTENT_FORMAT_APP_CBOR,
      .payload = request,
      .len = len,
      .cb = on_reply,
  };
  k_sem_reset(&reply_sem);
  int err = coap_client_req(&client, sock, porta, &req, NULL);
  if (err < 0) {
    printk("agent: coap request failed (%d)\n", err);
    return false;
  }
  if (k_sem_take(&reply_sem, K_MSEC(TIMEOUT_MS)) != 0) {
    coap_client_cancel_requests(&client);
    printk("agent: no reply in %d ms\n", TIMEOUT_MS);
    return false;
  }
  if (reply_code < 0) {
    printk("agent: coap error %d\n", reply_code);
    return false;
  }
  if (reply_code != COAP_RESPONSE_CODE_CHANGED) {
    printk("agent: coap %d.%02d\n", reply_code >> 5, reply_code & 0x1f);
    return false;
  }
  msg_decode_reply(reply, reply_len, out, on_entry, NULL);
  if (out->kind == MSG_ERR) {
    printk("agent: porta refused: %.*s\n", (int)out->text_len, out->text);
    return false;
  }
  return out->kind != MSG_BAD;
}

// One socket for the agent's life: coap_client keeps polling a finished
// request's fd, so closing it per check-in let a reused fd number deliver a
// stale error (-EINVAL) into the next exchange.
static int sock = -1;
// The build's family, the VM's rule (the fork's socket_address_zephyr.h):
// IPv6 when the build has it (Thread), else IPv4 (WiFi).
#if defined(CONFIG_NET_IPV6)
#define PORTA_FAMILY AF_INET6
static struct sockaddr_in6 porta = {.sin6_family = AF_INET6};
#define PORTA_PORT porta.sin6_port
#define PORTA_ADDR porta.sin6_addr
#else
#define PORTA_FAMILY AF_INET
static struct sockaddr_in porta = {.sin_family = AF_INET};
#define PORTA_PORT porta.sin_port
#define PORTA_ADDR porta.sin_addr
#endif

// --- Install ------------------------------------------------------------------

static const struct stage_io* staging;  // NULL: installs are off (said at start)
static uint8_t pubkey[ED25519_PUB_SIZE];
static struct store_stage stage;        // stage.state 0: nothing staged
static size_t fetch_offset;
static bool fetch_offset_known;         // false after a boot: recovered by stage_progress
static bool reboot_pending;
// When the link was asked to go down (ztoit_link_going_down), 0 if it was not.
static atomic_val_t going_down_at;

static const char* hex8(const uint8_t* hash) {
  static char out[2 * 4 + 1];
  for (int i = 0; i < 4; i++) snprintf(out + 2 * i, 3, "%02x", hash[i]);
  return out;
}

// A fetch that ends without a reboot (refused, dropped) pulls again from
// cursor 0, so goals passed over while it held staging come again. Set while a
// batch is decoded, so check_in applies it after the batch.
static bool resync_pending;
static void resync(void) {
  resync_pending = true;
}

static void refused_report(const char* name);

static void refuse(const char* why) {
  printk("agent: %s %s refused: %s\n", stage.name, hex8(stage.hash), why);
  staging->erase(staging->ctx);
  stage.state = STAGE_REFUSED;
  store_stage_save(&stage);
  refused_report(stage.name);
  resync();
}

// A goal/apps/<name> row: does it start, continue or replace a fetch?
static void consider(const char* name, const struct msg_app* app) {
  struct store_app installed;
  bool same_name = stage.state != 0 && strcmp(stage.name, name) == 0;
  if (store_app_load(name, &installed) && memcmp(installed.hash, app->hash, STAGE_HASH_SIZE) == 0) {
    if (same_name && (stage.state == STAGE_FETCHING || stage.state == STAGE_REFUSED)) {
      printk("agent: %s: the goal is the installed image again: %s dropped\n", name,
             stage.state == STAGE_REFUSED ? "refusal" : "fetch");
      if (stage.state == STAGE_REFUSED) refused_report(name);  // #running again
      stage.state = 0;
      store_stage_clear();
      resync();
    }
    return;
  }
  if (same_name && memcmp(stage.hash, app->hash, STAGE_HASH_SIZE) == 0) {
    if (stage.state == STAGE_REFUSED) printk("agent: %s %s was refused: not retried\n", name, hex8(app->hash));
    return;  // fetching: continues below; verified: the reboot is pending
  }
  if (stage.state == STAGE_FETCHING && !same_name) return;  // one at a time (resync brings this back)
  if (staging == NULL) {
    printk("agent: goal/apps/%s: installs are off\n", name);
    return;
  }
  // Erased before the record names the new image: a power cut in between
  // leaves the old record over erased staging, which resumes from 0.
  if (staging->erase(staging->ctx) != 0) {
    printk("agent: staging erase failed\n");
    return;
  }
  // A refusal this replaces is no longer said: obs/apps of its name goes again.
  if (stage.state == STAGE_REFUSED) refused_report(stage.name);
  memset(&stage, 0, sizeof(stage));
  strcpy(stage.name, name);
  memcpy(stage.hash, app->hash, STAGE_HASH_SIZE);
  memcpy(stage.sig, app->sig, ED25519_SIG_SIZE);
  stage.size = app->size;
  stage.keep = app->keep;
  stage.state = STAGE_FETCHING;
  fetch_offset = 0;
  fetch_offset_known = true;
  if (app->size > (int64_t)staging->size) {
    printk("agent: %s %s is %lld B, staging %u B\n", name, hex8(app->hash), (long long)app->size,
           (unsigned)staging->size);
    refuse("too big");
    return;
  }
  store_stage_save(&stage);
  printk("agent: install %s %s, %lld B%s: fetching\n", name, hex8(app->hash), (long long)app->size,
         app->keep ? "" : ", run once");
}

static void on_row(int64_t seq, const char* path, size_t path_len, int64_t kind, const uint8_t* value,
                   size_t value_len, void* ctx) {
  printk("agent: row %lld %.*s kind %lld, %u B\n", (long long)seq, (int)path_len, path, (long long)kind,
         (unsigned)value_len);
  static const char prefix[] = "goal/apps/";
  size_t name_len = path_len - (sizeof(prefix) - 1);
  if (path_len < sizeof(prefix) || memcmp(path, prefix, sizeof(prefix) - 1) != 0 || kind != 2) return;  // cell
  char name[STORE_NAME_MAX + 1];
  struct msg_app app;
  if (name_len > STORE_NAME_MAX || memchr(path + sizeof(prefix) - 1, '/', name_len)) {
    printk("agent: %.*s: not an app name the agent keeps\n", (int)path_len, path);
    return;
  }
  memcpy(name, path + sizeof(prefix) - 1, name_len);
  name[name_len] = 0;
  if (!msg_decode_app(value, value_len, &app)) {
    printk("agent: goal/apps/%s: not an App with a sig and a sha256 image\n", name);
    return;
  }
  consider(name, &app);
}

static struct {
  int16_t code;
  size_t len;
  bool last;
} block;
static uint8_t block_data[STAGE_BLOCK];
static const struct coap_client_request block_match = {.method = COAP_METHOD_GET};

// One Block2 block. The client would fetch the next block itself, but it
// cannot start a transfer at block n > 0 (its block context starts at 0), so
// the agent asks block by block and cancels the client's own follow-up.
static void on_block(const struct coap_client_response_data* data, void* user_data) {
  int16_t code = data->result_code;
  const uint8_t* payload = data->payload;
  size_t len = data->payload_len;
  bool last = data->last_block;
  block.code = code;
  block.last = last;
  block.len = 0;
  if (code == COAP_RESPONSE_CODE_CONTENT && len <= sizeof(block_data)) {
    if (len) memcpy(block_data, payload, len);
    block.len = len;
  } else if (code == COAP_RESPONSE_CODE_CONTENT) {
    block.code = -EMSGSIZE;
  }
  if (!last && code >= 0) coap_client_cancel_request(&client, (struct coap_client_request*)&block_match);
  k_sem_give(&reply_sem);
}

static bool get_block(const char* path, uint32_t num) {
  // RFC 7959 §2.2: NUM, M = 0, SZX.
  uint32_t value = num << 4 | coap_bytes_to_block_size(STAGE_BLOCK);
  struct coap_client_request req = {
      .method = COAP_METHOD_GET,
      .confirmable = true,
      .cb = on_block,
      .options = {{.code = COAP_OPTION_BLOCK2}},
      .num_options = 1,
  };
  struct coap_client_option* option = &req.options[0];
  for (uint32_t v = value; v != 0 || option->len == 0; v >>= 8) option->len++;
  for (int i = 0; i < option->len; i++) option->value[i] = value >> (8 * (option->len - 1 - i));
  if (strlen(path) >= sizeof(req.path)) {
    printk("agent: path of %u B over the client's %u\n", (unsigned)strlen(path), (unsigned)sizeof(req.path));
    return false;
  }
  strcpy(req.path, path);
  k_sem_reset(&reply_sem);
  int err = coap_client_req(&client, sock, (struct sockaddr*)&porta, &req, NULL);
  if (err < 0) {
    printk("agent: blob request failed (%d)\n", err);
    return false;
  }
  if (k_sem_take(&reply_sem, K_MSEC(TIMEOUT_MS)) != 0) {
    coap_client_cancel_requests(&client);
    printk("agent: no blob block in %d ms\n", TIMEOUT_MS);
    return false;
  }
  if (block.code != COAP_RESPONSE_CODE_CONTENT) {
    if (block.code < 0) printk("agent: blob coap error %d\n", block.code);
    else printk("agent: blob coap %d.%02d\n", block.code >> 5, block.code & 0x1f);
    return false;
  }
  return true;
}

static void verify(void) {
  printk("agent: %s %s fetched, %u B: verifying\n", stage.name, hex8(stage.hash), (unsigned)fetch_offset);
  if (stage.size >= 0 && stage.size != (int64_t)fetch_offset) {
    refuse("not the BlobRef's size");
    return;
  }
  stage.size = fetch_offset;
  switch (stage_verify(staging, fetch_offset, stage.hash, stage.sig, pubkey)) {
    case STAGE_OK:
      stage.state = STAGE_VERIFIED;
      store_stage_save(&stage);
      reboot_pending = true;
      printk("agent: %s %s verified: reboot to install after this check-in\n", stage.name, hex8(stage.hash));
      return;
    case STAGE_BAD_HASH: refuse("SHA-256 is not the goal's hash"); return;
    case STAGE_BAD_SIG: refuse("bad signature"); return;
    case STAGE_IO: refuse("staging read failed"); return;
  }
}

// Fetches blocks until the image is staged, an exchange fails, or the link has
// been going down for TIMEOUT_MS: ztoit_link_going_down waits 2 * TIMEOUT_MS +
// 1 s for this check-in, which leaves the block in flight time to finish.
static void fetch(void) {
  if (staging == NULL || stage.state != STAGE_FETCHING) return;
  if (!fetch_offset_known) {
    fetch_offset = stage_progress(staging);
    fetch_offset_known = true;
    printk("agent: %s %s: staged %u B, fetching from there\n", stage.name, hex8(stage.hash),
           (unsigned)fetch_offset);
  }
  char path[sizeof("blob/") + 2 * STAGE_HASH_SIZE];
  BUILD_ASSERT(sizeof(path) <= MAX_PATH_SIZE, "CONFIG_COAP_CLIENT_MAX_PATH_LENGTH is under the blob path");
  strcpy(path, "blob/");
  for (int i = 0; i < STAGE_HASH_SIZE; i++) snprintf(path + 5 + 2 * i, 3, "%02x", stage.hash[i]);
  size_t from = fetch_offset;
  int64_t start = k_uptime_get();
  while (true) {
    atomic_val_t down = atomic_get(&going_down_at);
    if (down != 0 && k_uptime_get() - down > TIMEOUT_MS) break;
    if (!get_block(path, fetch_offset / STAGE_BLOCK)) break;
    if (fetch_offset + block.len > staging->size) {
      refuse("bigger than staging");
      return;
    }
    if (block.len && staging->write(staging->ctx, fetch_offset, block_data, block.len) != 0) {
      printk("agent: staging write failed at %u\n", (unsigned)fetch_offset);
      break;
    }
    fetch_offset += block.len;
    if (block.last) {
      verify();
      return;
    }
    if (block.len != STAGE_BLOCK) {
      printk("agent: blob block of %u B, not the last\n", (unsigned)block.len);
      break;
    }
  }
  printk("agent: fetch %s %u..%u B in %lld ms\n", stage.name, (unsigned)from, (unsigned)fetch_offset,
         (long long)(k_uptime_get() - start));
}

// --- obs/apps --------------------------------------------------------------------

static struct {
  char name[STORE_NAME_MAX + 1];
  struct store_app app;
  int64_t seq;  // 0 until pushed; then resent with the same seq until acked
} report;

static void find_unreported(const char* name, struct store_app* app, void* ctx) {
  if (report.name[0] == 0 && !app->reported) {
    strcpy(report.name, name);
    report.app = *app;
  }
}

// obs/apps/<name> keeps the installed image (custos reads it as running), so a
// refused new image under that name is said by the lifecycle alone: #refused
// while the stage holds the refusal (#38). A refused first install has no record.
static const char* obs_lifecycle(const char* name, const struct store_app* app) {
  if (stage.state == STAGE_REFUSED && strcmp(stage.name, name) == 0) return "refused";
  return app->state == APP_ERASED ? "erased" : "running";
}

// The record goes unreported; a push of it in flight is dropped, so the
// #refused one goes with a new seq rather than resending the old one's.
static void refused_report(const char* name) {
  store_app_unreport(name);
  if (strcmp(report.name, name) == 0) memset(&report, 0, sizeof(report));
}

static void push_obs_app(void) {
  if (report.name[0] == 0) store_apps_each(find_unreported, NULL);
  if (report.name[0] == 0) return;
  if (report.seq == 0) report.seq = ++node_seq;
  struct msg_reply r;
  int64_t mono = k_uptime_get();
  const char* lifecycle = obs_lifecycle(report.name, &report.app);
  size_t len = msg_push_app(request, sizeof(request), id, bid, mono, report.seq, report.name, report.app.hash,
                            report.app.size, lifecycle);
  if (len && exchange(sock, (struct sockaddr*)&porta, len, &r, NULL) && r.kind == MSG_ACK) {
    hw = r.hw;
    if (hw >= report.seq) {
      report.app.reported = 1;
      store_app_save(report.name, &report.app);
      printk("agent: obs/apps/%s pushed: %s #%s\n", report.name, hex8(report.app.hash), lifecycle);
      memset(&report, 0, sizeof(report));
    }
  }
}

// --- obs/<name> ------------------------------------------------------------------

// Posted from the VM thread, pushed from the agent's.
static K_MUTEX_DEFINE(obs_lock);
static struct obs_table obs;

// The fork's zephyr.observe primitive: 0, -EINVAL, -ENOSPC (obs_post).
int ztoit_observe(const uint8_t* name, size_t length, int64_t value) {
  k_mutex_lock(&obs_lock, K_FOREVER);
  int err = obs_post(&obs, (const char*)name, length, value, k_uptime_get());
  k_mutex_unlock(&obs_lock);
  return err;
}

// Each unacked cell once, so a check-in is at most OBS_CELLS of these
// exchanges however often the app posts; the first that fails ends them.
static void push_obs(void) {
  for (int i = 0; i < OBS_CELLS; i++) {
    struct obs_cell cell;
    k_mutex_lock(&obs_lock, K_FOREVER);
    bool any = obs_next(&obs, &node_seq, &cell);
    k_mutex_unlock(&obs_lock);
    if (!any) return;
    struct msg_reply r;
    int64_t mono = k_uptime_get();
    size_t len =
        msg_push_obs(request, sizeof(request), id, bid, mono, cell.seq, cell.name, cell.value, cell.captured);
    if (!len || !exchange(sock, (struct sockaddr*)&porta, len, &r, NULL) || r.kind != MSG_ACK) return;
    hw = r.hw;
    k_mutex_lock(&obs_lock, K_FOREVER);
    obs_ack(&obs, hw);
    k_mutex_unlock(&obs_lock);
    printk("agent: obs/%s pushed: %lld\n", cell.name, (long long)cell.value);
  }
}

// --- tel/print ------------------------------------------------------------------

// Written from the VM thread (ztoit_console), read from the agent's (push_console).
static K_MUTEX_DEFINE(console_lock);
static struct console console;  // console.ring NULL until agent_start: writes are dropped

// The fork's stdout and stderr primitives: every byte they write, '\n' ending
// a line. Never prints: this is the console's own path.
void ztoit_console(const uint8_t* bytes, size_t length) {
  k_mutex_lock(&console_lock, K_FOREVER);
  console_write(&console, bytes, length, k_uptime_get());
  k_mutex_unlock(&console_lock);
}

static void start_console(void) {
  const struct console_ring* ring = store_console_ring();
  uint32_t acked = store_console_acked_load();
  k_mutex_lock(&console_lock, K_FOREVER);
  console_start(&console, ring, (uint32_t)bid, acked);
  uint32_t next = console.next;
  k_mutex_unlock(&console_lock);
  if (ring) printk("agent: console ring: line %u next, %u acked\n", (unsigned)next, (unsigned)acked);
}

#define CONSOLE_PUSHES 2

// Up to CONSOLE_PUSHES pushes, each as many lines as MSG_PUSH_MAX holds; the
// first that fails ends them, and the rest ride the next check-in. A push's
// rows take fresh node seqs, also when they are resent: porta drops a seq at or
// under its high-water, so a resend under an old seq after a later push was
// acked would be lost. A lost ack costs a line pushed twice instead.
static void push_console(void) {
  static struct console_batch batch;  // its text is MSG_PUSH_MAX: not on the agent's stack
  for (int i = 0; i < CONSOLE_PUSHES; i++) {
    k_mutex_lock(&console_lock, K_FOREVER);
    console_select(&console, &batch);
    k_mutex_unlock(&console_lock);
    if (batch.n == 0) return;
    size_t n = batch.n;
    int64_t first = node_seq + 1;
    struct msg_reply r;
    size_t len = msg_push_prints(request, sizeof(request), id, bid, k_uptime_get(), first, batch.rows, &n);
    if (!len) return;  // cannot be: a row of CONSOLE_LINE_MAX fits (console_test)
    node_seq += n;
    if (!exchange(sock, (struct sockaddr*)&porta, len, &r, NULL) || r.kind != MSG_ACK) return;
    hw = r.hw;
    if (hw < node_seq) return;
    uint32_t through = batch.through[n - 1];
    k_mutex_lock(&console_lock, K_FOREVER);
    console_ack(&console, through);
    k_mutex_unlock(&console_lock);
    store_console_acked_save(through);
    printk("agent: tel/print pushed: %u rows, through line %u\n", (unsigned)n, (unsigned)through);
  }
}

// --- Check-in --------------------------------------------------------------------

static void check_in(void) {
  int64_t start = k_uptime_get();
  struct msg_reply r;
  wak++;
  size_t len = msg_pull(request, sizeof(request), id, cursor, bid, hw, wak);
  if (len && exchange(sock, (struct sockaddr*)&porta, len, &r, on_row) && r.kind == MSG_BATCH) {
    if (r.hi > cursor) cursor = r.hi;
    printk("agent: pull ok %lld ms: %d rows, cursor %lld\n", (long long)(k_uptime_get() - start), r.entries,
           (long long)cursor);
    if (!build_acked) {
      static const struct msg_build build = {
          .sdk = ZTOIT_SDK, .chip = CONFIG_SOC, .board = CONFIG_BOARD, .build = ZTOIT_BUILD};
      int64_t mono = k_uptime_get();
      len = msg_push_build(request, sizeof(request), id, bid, mono, BUILD_SEQ, &build);
      if (len && exchange(sock, (struct sockaddr*)&porta, len, &r, NULL) && r.kind == MSG_ACK) {
        hw = r.hw;
        build_acked = hw >= BUILD_SEQ;
        printk("agent: push ok %lld ms: hw %lld\n", (long long)(k_uptime_get() - mono), (long long)hw);
      }
    }
    push_obs_app();
    push_obs();
    push_console();
    fetch();
  }
  if (resync_pending) cursor = 0;
  resync_pending = false;
}

// --- Thread -------------------------------------------------------------------

static void owe_check_in(void) {
  k_sem_reset(&settled_sem);
  atomic_set(&going_down_at, 0);
  atomic_set(&owed, 1);
  k_sem_give(&addr_sem);
}

// Thread: the fork's thread_up, on each net.open that brings the link up.
// The first after a boot comes before the attach (thread_up only starts it):
// the check-in waits for it in attached().
void ztoit_link_up(void) {
  owe_check_in();
}

#if defined(CONFIG_NET_L2_OPENTHREAD)
// Thread: true once the node is a child or better, false after TIMEOUT_MS.
// Without the wait the first check-in after each boot failed (-115) and the
// goal waited a whole net.open cycle (#32).
static bool attached(void) {
  otInstance* instance = openthread_get_default_instance();
  int64_t start = k_uptime_get();
  while (true) {
    openthread_mutex_lock();
    otDeviceRole role = otThreadGetDeviceRole(instance);
    openthread_mutex_unlock();
    if (role >= OT_DEVICE_ROLE_CHILD) return true;
    if (k_uptime_get() - start > TIMEOUT_MS) return false;
    k_sleep(K_MSEC(100));
  }
}
#endif

#if !defined(CONFIG_NET_IPV6)
// WiFi: the lease, which comes after wifi_up returns.
static struct net_mgmt_event_callback addr_cb;
static void on_addr(struct net_mgmt_event_callback* cb, uint64_t event, struct net_if* iface) {
  owe_check_in();
}
#endif

// The fork's wifi_down calls this before disconnecting. Without it an app that
// closes the network straight after its own exchange cut the agent's check-in
// off mid-flight (3 of 17 cycles, -EINVAL on the retransmit). Bounded: before
// its fetch a check-in is at most CHECK_IN_EXCHANGES exchanges of TIMEOUT_MS
// each, and a fetch stops starting blocks TIMEOUT_MS after this call (fetch).
// The pull, obs/build, obs/apps, each obs/<name> cell (push_obs), then the
// console's pushes (push_console).
#define CHECK_IN_EXCHANGES (3 + OBS_CELLS + CONSOLE_PUSHES)
void ztoit_link_going_down(void) {
  atomic_set(&going_down_at, (atomic_val_t)k_uptime_get());
  if (atomic_get(&owed) && k_sem_take(&settled_sem, K_MSEC(CHECK_IN_EXCHANGES * TIMEOUT_MS + 1000)) != 0) {
    printk("agent: link going down mid check-in\n");
  }
}

// The id is the factory link address, lowercase hex (PROTOCOL-NSL §5.3).
static bool read_id(void) {
  struct net_if* iface = net_if_get_default();
  struct net_linkaddr* link = iface ? net_if_get_link_addr(iface) : NULL;
  if (!link || link->len == 0 || link->len > 8) return false;
  for (int i = 0; i < link->len; i++) snprintf(id + 2 * i, 3, "%02x", link->addr[i]);
  return true;
}

// Installs need porta's key and a staging partition; without either the
// agent still checks in, and says so.
static void set_up_installs(void) {
  if (CONFIG_TOIT_AGENT_PUBKEY[0] == 0) {
    printk("agent: no CONFIG_TOIT_AGENT_PUBKEY: nothing installs\n");
  } else if (!stage_parse_key(CONFIG_TOIT_AGENT_PUBKEY, pubkey)) {
    printk("agent: CONFIG_TOIT_AGENT_PUBKEY is not 64 hex digits: nothing installs\n");
  } else if ((staging = store_staging()) == NULL) {
    printk("agent: no staging partition: nothing installs\n");
  }
  if (staging == NULL) return;
  if (store_stage_load(&stage) && stage.state == STAGE_FETCHING) {
    printk("agent: install %s %s: resuming the fetch\n", stage.name, hex8(stage.hash));
  }
}

static void agent_main(void* a, void* b, void* c) {
  if (!read_id()) {
    printk("agent: no link address: no check-ins\n");  // the id is never configured or defaulted
    return;
  }
  PORTA_PORT = htons(CONFIG_TOIT_AGENT_PORT);
  if (zsock_inet_pton(PORTA_FAMILY, CONFIG_TOIT_AGENT_PORTA, &PORTA_ADDR) != 1) {
    printk("agent: bad porta address '%s': no check-ins\n", CONFIG_TOIT_AGENT_PORTA);
    return;
  }
  sock = zsock_socket(PORTA_FAMILY, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    printk("agent: socket failed (%d): no check-ins\n", errno);
    return;
  }
  set_up_installs();
  coap_client_init(&client, "agent");
#if !defined(CONFIG_NET_IPV6)
  net_mgmt_init_event_callback(&addr_cb, on_addr, NET_EVENT_IPV4_ADDR_ADD);
  net_mgmt_add_event_callback(&addr_cb);
#endif
  printk("agent: id %s, boot %lld, porta %s:%d\n", id, (long long)bid, CONFIG_TOIT_AGENT_PORTA,
         CONFIG_TOIT_AGENT_PORT);
  while (true) {
    k_sem_take(&addr_sem, K_FOREVER);
#if defined(CONFIG_NET_L2_OPENTHREAD)
    if (!attached()) printk("agent: not attached after %d ms: checking in anyway\n", TIMEOUT_MS);
#endif
    check_in();
    atomic_set(&owed, 0);
    k_sem_give(&settled_sem);
    if (reboot_pending) {
      printk("agent: rebooting to install %s\n", stage.name);
#ifdef CONFIG_WIFI_ESP32
      // A reboot while associated is a CPU-only reset (SW_CPU_RESET) that
      // leaves the radio's state behind: on Zephyr 4.5 every later connect then
      // fails with "AP not found" until a hard reset (#47). ESP-IDF stops WiFi
      // in a shutdown handler; Zephyr's port registers none.
      esp_wifi_stop();
#endif
      sys_reboot(SYS_REBOOT_COLD);
    }
  }
}

// The stack is in .bss, not in K_THREAD_DEFINE's .noinit: on the ESP32 that
// is dram1 (CONFIG_ESP32_REGION_1_NOINIT), which the Toit arena fills, and
// the ed25519 check needs a larger stack than a check-in (Kconfig).
static K_KERNEL_STACK_MEMBER(agent_stack, CONFIG_TOIT_AGENT_STACK_SIZE);
static struct k_thread agent;

// main() starts the agent once the registry and the install records are
// settled (store.h): before that it must not read or write them. The boot is
// counted and the console started here, before the VM, so its first lines are
// kept with this boot's id.
void agent_start(void) {
  count_boot();
  start_console();
  k_thread_create(&agent, agent_stack, K_KERNEL_STACK_SIZEOF(agent_stack), agent_main, NULL, NULL, NULL,
                  K_LOWEST_APPLICATION_THREAD_PRIO, 0, K_NO_WAIT);
  k_thread_name_set(&agent, "agent");
}
