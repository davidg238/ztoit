// Debug build only (west build ... -- -DHFCLK_HOLD=ON), nRF: keep the HF crystal
// requested from boot, as the USB device stack does on the dongle, on a board
// without USB (ztoit#23). The radio driver's own clock request then completes
// inside the request instead of from the clock interrupt.
#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/sys/onoff.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>

static struct onoff_client hold;

static int start_hfclk_hold(void) {
  sys_notify_init_spinwait(&hold.notify);
  int err = onoff_request(z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_HF), &hold);
  printk("dbg: hfclk held %d\n", err);
  return 0;
}

SYS_INIT(start_hfclk_hold, APPLICATION, 98);
