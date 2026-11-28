/* Test-only (ztoit#62): a wire between two emulated GPIO pins, so that the
 * Toit gpio tests (tests/qemu/gpio-test.toit) see one pin's output as the
 * other's input. Built when the devicetree has a `zephyr,gpio-emul`
 * controller (zephyr/boards/qemu_riscv32.overlay), the QEMU test build only.
 *
 * The wire joins the two pins of `/zephyr,user`'s `wire-gpios` (the overlay).
 * A pin that is an output drives the other pin's input; a pin that is
 * not driven reads its own pull, or keeps its last value without one.
 *
 * gpio_emul calls a callback after every configure of a pin and after a write
 * to a pin that is an output and not an input; settle() then recomputes both
 * inputs. A write to a pin configured as input and output is not seen (the
 * emulator only raises the pin's own interrupt), so the wire does not carry it.
 */
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/init.h>

#define WIRE DT_PATH(zephyr_user)

static const struct gpio_dt_spec ends[2] = {
	GPIO_DT_SPEC_GET_BY_IDX(WIRE, wire_gpios, 0),
	GPIO_DT_SPEC_GET_BY_IDX(WIRE, wire_gpios, 1),
};
static struct gpio_callback callbacks[2];

static void settle(void)
{
	gpio_flags_t flags[2];

	for (int i = 0; i < 2; i++) {
		gpio_emul_flags_get(ends[i].port, ends[i].pin, &flags[i]);
	}
	for (int i = 0; i < 2; i++) {
		const struct gpio_dt_spec *me = &ends[i];
		const struct gpio_dt_spec *other = &ends[1 - i];
		int value;

		if (!(flags[i] & GPIO_INPUT) || (flags[i] & GPIO_OUTPUT)) {
			continue; /* Not an input, or its own output drives it. */
		}
		if (flags[1 - i] & GPIO_OUTPUT) {
			gpio_port_value_t out;

			gpio_emul_output_get_masked(other->port, BIT(other->pin), &out);
			value = (out & BIT(other->pin)) != 0;
		} else if (flags[i] & GPIO_PULL_UP) {
			value = 1;
		} else if (flags[i] & GPIO_PULL_DOWN) {
			value = 0;
		} else {
			continue;
		}
		/* Raises the pin's interrupt when the value changes. */
		gpio_emul_input_set(me->port, me->pin, value);
	}
}

static void changed(const struct device *port, struct gpio_callback *cb, gpio_port_pins_t pins)
{
	settle();
}

static int wire_init(void)
{
	for (int i = 0; i < 2; i++) {
		if (!gpio_is_ready_dt(&ends[i])) {
			return -ENODEV;
		}
		gpio_init_callback(&callbacks[i], changed, BIT(ends[i].pin));
		gpio_add_callback(ends[i].port, &callbacks[i]);
	}
	return 0;
}

SYS_INIT(wire_init, APPLICATION, 0);
