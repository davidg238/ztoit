// Debug build only (west build ... -- -DFATAL_DUMP=ON), nRF: tell a silent hang
// apart (ztoit#23). The console is interrupt-driven USB CDC, so a fatal error
// (which halts with interrupts locked) prints nothing. Three outcomes:
//  - fatal error: saved to RAM that survives the reboot, printed at next boot;
//  - spin with interrupts locked or in an ISR: the watchdog resets, and the
//    reset cause is printed at next boot;
//  - threads deadlocked or starved: `dbg: alive` keeps printing.
#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/fatal.h>
#include <zephyr/sys/reboot.h>
#include <string.h>
#include <nrf.h>

#define DUMP_MAGIC 0x23fa7a1dU
#define STACK_WORDS 24

static __noinit struct {
  uint32_t magic;
  uint32_t reason;
  uint32_t pc, lr, psr;
  uint32_t in_isr;
  int64_t uptime_ms;
  char thread[16];
  uint32_t stack[STACK_WORDS];  // Above the exception frame: return addresses to unwind by hand.
} dump;

void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf* esf) {
  dump.reason = reason;
  dump.in_isr = k_is_in_isr();
  dump.uptime_ms = k_uptime_get();
  dump.pc = dump.lr = dump.psr = 0;
  memset(dump.stack, 0, sizeof(dump.stack));
  if (esf != NULL) {
    dump.pc = esf->basic.pc;
    dump.lr = esf->basic.lr;
    dump.psr = esf->basic.xpsr;
    memcpy(dump.stack, (const uint32_t*)(&esf->basic + 1), sizeof(dump.stack));
  }
#ifdef FATAL_DUMP_PEEK
  // The nRF radio driver's static state, at addresses read from this build's
  // ELF with nm (check them after every rebuild): the last two stack words carry
  // hfclk_is_running..m_state (9 bytes at PEEK_CORE) and m_trx_state.
  memcpy(&dump.stack[STACK_WORDS - 3], (const void*)PEEK_CORE, 9);
  dump.stack[STACK_WORDS - 3 + 2] = (dump.stack[STACK_WORDS - 3 + 2] & 0xff) | (*(const uint8_t*)PEEK_TRX << 8);
#endif
  const char* name = k_thread_name_get(k_current_get());
  strncpy(dump.thread, name ? name : "?", sizeof(dump.thread) - 1);
  dump.thread[sizeof(dump.thread) - 1] = 0;
  dump.magic = DUMP_MAGIC;
  // The bootloader may overwrite RAM: GPREGRET2 carries at least the reason.
  NRF_POWER->GPREGRET2 = 0xA0 | (reason & 0xf);
  sys_reboot(SYS_REBOOT_WARM);
}

static void feed(struct k_timer* timer) {
  NRF_WDT->RR[0] = WDT_RR_RR_Reload;
}
static K_TIMER_DEFINE(feed_timer, feed, NULL);

static void alive(struct k_work* work);
static K_WORK_DELAYABLE_DEFINE(alive_work, alive);

static void alive(struct k_work* work) {
  printk("dbg: alive %lld s\n", k_uptime_get() / 1000);
  k_work_reschedule(&alive_work, K_SECONDS(10));
}

static void report(void) {
  // RESETREAS: 1 pin, 2 watchdog, 4 soft reset (the fatal handler's reboot), 8 lockup.
  uint32_t cause = NRF_POWER->RESETREAS;
  NRF_POWER->RESETREAS = cause;
  printk("dbg: resetreas 0x%x gpregret2 0x%x\n", cause, (unsigned)NRF_POWER->GPREGRET2);
  NRF_POWER->GPREGRET2 = 0;
  if (dump.magic == DUMP_MAGIC) {
    printk("dbg: FATAL reason %u pc %08x lr %08x psr %08x isr %u thread %s at %lld ms\n",
           dump.reason, dump.pc, dump.lr, dump.psr, dump.in_isr, dump.thread, dump.uptime_ms);
    for (int i = 0; i < STACK_WORDS; i += 8) {
      printk("dbg: stack %08x %08x %08x %08x %08x %08x %08x %08x\n",
             dump.stack[i], dump.stack[i + 1], dump.stack[i + 2], dump.stack[i + 3],
             dump.stack[i + 4], dump.stack[i + 5], dump.stack[i + 6], dump.stack[i + 7]);
    }
    dump.magic = 0;
  }
}

static int start_fatal_dump(void) {
  NRF_WDT->CRV = 8 * 32768;  // 8 s.
  NRF_WDT->RREN = WDT_RREN_RR0_Msk;
  NRF_WDT->CONFIG = WDT_CONFIG_SLEEP_Run << WDT_CONFIG_SLEEP_Pos;
  NRF_WDT->TASKS_START = 1;
  k_timer_start(&feed_timer, K_SECONDS(1), K_SECONDS(1));
  // Hold the boot: the CDC console drops output until the host has opened it,
  // and the fatal error being chased strikes about 2 s after boot.
  k_sleep(K_SECONDS(4));
  report();
  k_work_schedule(&alive_work, K_SECONDS(10));
  return 0;
}

SYS_INIT(start_fatal_dump, APPLICATION, 99);
