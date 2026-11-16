// Debug build only (west build ... -- -DOT_COUNTERS=ON): print OpenThread's MAC
// counters once a minute, to line the node's own TX outcomes up against the
// soak's UDP losses (ztoit#18 TX loss). Nothing is printed until Thread is up.
#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/net/openthread.h>
#include <openthread/link.h>

static void print_counters(struct k_work* work);
static K_WORK_DELAYABLE_DEFINE(counters_work, print_counters);

static void print_counters(struct k_work* work) {
  otInstance* instance = openthread_get_default_instance();
  if (instance != NULL) {
    openthread_mutex_lock();
    otMacCounters c = *otLinkGetCounters(instance);
    openthread_mutex_unlock();
    printk("ot-mac: tx=%u ackreq=%u acked=%u retry=%u maxretry=%u cca=%u abort=%u busy=%u "
           "rx=%u dup=%u\n",
           c.mTxTotal, c.mTxAckRequested, c.mTxAcked, c.mTxRetry, c.mTxDirectMaxRetryExpiry,
           c.mTxErrCca, c.mTxErrAbort, c.mTxErrBusyChannel, c.mRxTotal, c.mRxDuplicated);
  }
  k_work_reschedule(&counters_work, K_SECONDS(60));
}

static int start_counters(void) {
  k_work_schedule(&counters_work, K_SECONDS(60));
  return 0;
}

SYS_INIT(start_counters, APPLICATION, 99);
