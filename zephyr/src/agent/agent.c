// The C agent's check-in (nodus-toit design §5, rete #521): first slice.
//
// Piggyback (design §7): each time the network gains an address, which on
// WiFi is each net.open, the agent checks in with porta over CoAP:
//   POST /space #pull  -> #batch (logged; the cursor advances)
//   POST /space #push  -> #ack   (obs/build, once per boot, until acked)
// Not yet: applying goals, images, console, safe mode.
//
// State: `bid` is a boot counter persisted with Zephyr settings (rete
// PROTOCOL-NSL §5.3: porta tells a reboot from a lost ack by it); cursor, hw
// and wak are RAM, as on an nsl node, and resync from 0 after a reboot.
#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/net/coap_client.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/settings/settings.h>

#include "msg.h"

#define REPLY_MAX CONFIG_COAP_CLIENT_MESSAGE_SIZE
#define TIMEOUT_MS 15000

static struct coap_client client;
static K_SEM_DEFINE(addr_sem, 0, 1);
static K_SEM_DEFINE(reply_sem, 0, 1);
static struct net_mgmt_event_callback addr_cb;
// A check-in is owed from the moment the address arrives until it ends, so a
// link taken down in between waits for it (ztoit_link_going_down).
static atomic_t owed;
static K_SEM_DEFINE(settled_sem, 0, 1);

static char id[2 * 8 + 1];
static int64_t bid;
static int64_t cursor, hw, wak;
static bool build_acked;

static uint8_t request[256];
static uint8_t reply[REPLY_MAX];
static size_t reply_len;
static int16_t reply_code;

// --- Boot counter -------------------------------------------------------------

static int bid_loaded(const char* key, size_t len, settings_read_cb read, void* cb_arg, void* param) {
  if (len == sizeof(bid)) read(cb_arg, &bid, sizeof(bid));
  return 0;
}

static void count_boot(void) {
  int err = settings_subsys_init();
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

static void on_reply(int16_t code, size_t offset, const uint8_t* payload, size_t len, bool last,
                     void* user_data) {
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

static void on_row(int64_t seq, const char* path, size_t path_len, int64_t kind, const uint8_t* value,
                   size_t value_len, void* ctx) {
  printk("agent: row %lld %.*s kind %lld, %u B\n", (long long)seq, (int)path_len, path, (long long)kind,
         (unsigned)value_len);
}

// One socket for the agent's life: coap_client keeps polling a finished
// request's fd, so closing it per check-in let a reused fd number deliver a
// stale error (-EINVAL) into the next exchange.
static int sock = -1;
static struct sockaddr_in porta = {.sin_family = AF_INET};

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
      len = msg_push_build(request, sizeof(request), id, bid, mono, 1, &build);
      if (len && exchange(sock, (struct sockaddr*)&porta, len, &r, NULL) && r.kind == MSG_ACK) {
        hw = r.hw;
        build_acked = hw >= 1;
        printk("agent: push ok %lld ms: hw %lld\n", (long long)(k_uptime_get() - mono), (long long)hw);
      }
    }
  }
}

// --- Thread -------------------------------------------------------------------

static void on_addr(struct net_mgmt_event_callback* cb, uint64_t event, struct net_if* iface) {
  k_sem_reset(&settled_sem);
  atomic_set(&owed, 1);
  k_sem_give(&addr_sem);
}

// The fork's wifi_down calls this before disconnecting. Without it an app that
// closes the network straight after its own exchange cut the agent's check-in
// off mid-flight (3 of 17 cycles, -EINVAL on the retransmit). Bounded: a
// check-in is at most two exchanges.
void ztoit_link_going_down(void) {
  if (atomic_get(&owed) && k_sem_take(&settled_sem, K_MSEC(2 * TIMEOUT_MS + 1000)) != 0) {
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

static void agent_main(void* a, void* b, void* c) {
  if (!read_id()) {
    printk("agent: no link address: no check-ins\n");  // the id is never configured or defaulted
    return;
  }
  porta.sin_port = htons(CONFIG_TOIT_AGENT_PORT);
  if (zsock_inet_pton(AF_INET, CONFIG_TOIT_AGENT_PORTA, &porta.sin_addr) != 1) {
    printk("agent: bad porta address '%s': no check-ins\n", CONFIG_TOIT_AGENT_PORTA);
    return;
  }
  sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    printk("agent: socket failed (%d): no check-ins\n", errno);
    return;
  }
  count_boot();
  coap_client_init(&client, "agent");
  net_mgmt_init_event_callback(&addr_cb, on_addr, NET_EVENT_IPV4_ADDR_ADD);
  net_mgmt_add_event_callback(&addr_cb);
  printk("agent: id %s, boot %lld, porta %s:%d\n", id, (long long)bid, CONFIG_TOIT_AGENT_PORTA,
         CONFIG_TOIT_AGENT_PORT);
  while (true) {
    k_sem_take(&addr_sem, K_FOREVER);
    check_in();
    atomic_set(&owed, 0);
    k_sem_give(&settled_sem);
  }
}

K_THREAD_DEFINE(agent, CONFIG_TOIT_AGENT_STACK_SIZE, agent_main, NULL, NULL, NULL, K_LOWEST_APPLICATION_THREAD_PRIO, 0,
                0);
