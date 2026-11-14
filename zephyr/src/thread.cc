// ztoit Thread bring-up (builds with prj_thread.conf only). Same shape as rete's
// nodus/oevm/src/backend/zephyr/thread_bringup.c: run OpenThread, admin-up the
// net_if (openthread_run() does not, and sockets then fail with ENETDOWN), and
// block until the MTD attaches. Then one UDP round trip to the gateway's
// udp-echo6 (:7777) proves the mesh before the VM starts.
#include <zephyr/kernel.h>

#ifdef CONFIG_NET_L2_OPENTHREAD

#include <cstdio>
#include <cstring>
#include <zephyr/net/net_if.h>
#include <zephyr/net/openthread.h>
#include <zephyr/net/socket.h>
#include <openthread.h>
#include <openthread/thread.h>

#include "thread.h"

// The gateway's mesh-local EID (gw85224-01, OTBR leader).
static const char* const ECHO_HOST = "fdc5:9109:9afe:97ea:9541:d963:f8d9:8567";
static const int ECHO_PORT = 7777;

static bool attach() {
  openthread_init();
  int run = openthread_run();
  struct openthread_context* ctx = openthread_get_default_context();
  struct net_if* iface = ctx ? ctx->iface : net_if_get_default();
  int up = iface ? net_if_up(iface) : -1;
  printf("thread: run=%d net_if_up=%d, joining...\n", run, up);
  otInstance* instance = openthread_get_default_instance();
  int64_t start = k_uptime_get();
  for (int i = 0; i < 600 && instance; i++) {  // Up to 60 s.
    openthread_mutex_lock();
    otDeviceRole role = otThreadGetDeviceRole(instance);
    openthread_mutex_unlock();
    if (role == OT_DEVICE_ROLE_CHILD || role == OT_DEVICE_ROLE_ROUTER) {
      printf("thread: attached role=%d in %d ms\n", role, static_cast<int>(k_uptime_get() - start));
      return true;
    }
    k_msleep(100);
  }
  printf("thread: attach timed out\n");
  return false;
}

static void echo_once() {
  int fd = zsock_socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  if (fd < 0) { printf("thread: socket errno %d\n", errno); return; }
  struct sockaddr_in6 to = {};
  to.sin6_family = AF_INET6;
  to.sin6_port = htons(ECHO_PORT);
  zsock_inet_pton(AF_INET6, ECHO_HOST, &to.sin6_addr);
  struct timeval timeout = { .tv_sec = 3, .tv_usec = 0 };
  zsock_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  const char message[] = "ztoit echo";
  for (int attempt = 1; attempt <= 3; attempt++) {
    int64_t start = k_uptime_get();
    int sent = zsock_sendto(fd, message, sizeof(message) - 1, 0,
                            reinterpret_cast<struct sockaddr*>(&to), sizeof(to));
    if (sent < 0) { printf("thread: sendto errno %d\n", errno); k_msleep(1000); continue; }
    char buffer[64];
    int received = zsock_recv(fd, buffer, sizeof(buffer), 0);
    if (received == sizeof(message) - 1 && memcmp(buffer, message, received) == 0) {
      printf("thread: ECHO OK %d B in %d ms (attempt %d)\n", received,
             static_cast<int>(k_uptime_get() - start), attempt);
      break;
    }
    printf("thread: attempt %d no echo (recv %d, errno %d)\n", attempt, received, errno);
  }
  zsock_close(fd);
}

void ztoit_thread_up() {
  if (attach()) echo_once();
}

#endif  // CONFIG_NET_L2_OPENTHREAD
