// Toit's standard gpio library on the port (ztoit#62), under QEMU with the
// emulated controllers of zephyr/boards/qemu_riscv32.overlay: pins 0..7 and
// 32..39, pin 7 reserved, and pin 0 wired to pin 33 by tests/qemu/gpio-join.c.
// Written against `gpio` only; adapted from the fork's tests/hw gpio tests.
//
// What the emulator cannot show: open drain (gpio_emul refuses
// GPIO_OPEN_DRAIN, so the library's call must throw), and a write to a pin
// configured as input and output does not travel over the wire.
import gpio
import expect show *
import monitor show Semaphore

A ::= 0          // Port 0, pin 0.
B ::= 33         // Port 1, pin 1: wired to A.
RESERVED ::= 7   // The overlay's reserved-pins.

main:
  test-numbers
  test-in-out
  test-pulls
  test-wait-for
  test-open-drain
  test-close

test-numbers:
  expect-equals "INVALID_ARGUMENT" (catch: gpio.Pin -1)
  expect-equals "INVALID_ARGUMENT" (catch: gpio.Pin 8)    // Port 0 has 8 pins.
  expect-equals "INVALID_ARGUMENT" (catch: gpio.Pin 40)   // So has port 1.
  expect-equals "INVALID_ARGUMENT" (catch: gpio.Pin 64)   // No port 2.
  expect-equals "RESTRICTED_PIN" (catch: gpio.Pin RESERVED)
  reserved := gpio.Pin RESERVED --allow-restricted --input
  reserved.close
  a := gpio.Pin A
  expect-equals "ALREADY_IN_USE" (catch: gpio.Pin A)
  expect-equals "ALREADY_IN_USE" (catch: gpio.Pin A --allow-restricted)
  a.close

test-in-out:
  a := gpio.Pin A --output
  b := gpio.Pin B --input
  try:
    a.set 1
    expect-equals 1 b.get
    a.set 0
    expect-equals 0 b.get
    // The other way.
    a.configure --input
    b.configure --output --value=1
    expect-equals 1 a.get
    b.set 0
    expect-equals 0 a.get
    // An output's initial value.
    b.configure --input
    a.configure --output --value=1
    expect-equals 1 b.get
    // Input and output: the pin reads its own output.
    a.configure --input --output --value=0
    expect-equals 0 a.get
    a.set 1
    expect-equals 1 a.get
  finally:
    a.close
    b.close

test-pulls:
  a := gpio.Pin A --input --pull-up
  b := gpio.Pin B --input
  try:
    expect-equals 1 a.get
    a.set-pull --down
    expect-equals 0 a.get
    a.set-pull --up
    expect-equals 1 a.get
    // A driven wire beats the pull.
    b.configure --output --value=0
    expect-equals 0 a.get
    b.configure --input
    expect-equals 1 a.get
    a.configure --input --pull-down
    expect-equals 0 a.get
    expect-equals "INVALID_ARGUMENT" (catch: a.configure --input --pull-up --pull-down)
  finally:
    a.close
    b.close

test-wait-for:
  a := gpio.Pin A --output --value=0
  b := gpio.Pin B --input
  try:
    expect-equals DEADLINE-EXCEEDED-ERROR (catch: with-timeout --ms=50: b.wait-for 1)
    // Both edges, the level changing while the waiter waits.
    task::
      sleep --ms=10
      a.set 1
    with-timeout --ms=1000: b.wait-for 1
    task::
      sleep --ms=10
      a.set 0
    with-timeout --ms=1000: b.wait-for 0

    // A pulse over before the waiter runs again: only the edge shows it.
    done := Semaphore
    task::
      b.wait-for 1
      done.up
    sleep --ms=10
    a.set 1
    a.set 0
    expect-equals 0 b.get
    with-timeout --ms=1000: done.down
    a.set 1
    task::
      b.wait-for 0
      done.up
    sleep --ms=10
    a.set 0
    a.set 1
    expect-equals 1 b.get
    with-timeout --ms=1000: done.down
    a.set 0

    // Two waiters on one pin.
    2.repeat:
      task::
        b.wait-for 1
        done.up
    sleep --ms=10
    a.set 1
    with-timeout --ms=1000:
      done.down
      done.down
    a.set 0
    expect-equals DEADLINE-EXCEEDED-ERROR (catch: with-timeout --ms=50: b.wait-for 1)

    // Many edges, each waited for.
    20.repeat:
      task::
        sleep --ms=1
        a.set 1
      with-timeout --ms=1000: b.wait-for 1
      task::
        sleep --ms=1
        a.set 0
      with-timeout --ms=1000: b.wait-for 0

    // Close wakes a waiter.
    task::
      b.wait-for 1
      done.up
    sleep --ms=10
    b.close
    with-timeout --ms=1000: done.down
  finally:
    a.close
    b.close

test-open-drain:
  a := gpio.Pin A --output --value=1
  try:
    expect-not-null (catch: a.set-open-drain true)
    a.set 0  // Still a push-pull output.
  finally:
    a.close
  // A constructor that throws in configure releases the pin.
  expect-not-null (catch: gpio.Pin A --output --open-drain)
  (gpio.Pin A).close

test-close:
  100.repeat:
    (gpio.Pin A --input).close
  b := gpio.Pin B --input --pull-down
  try:
    a := gpio.Pin A --output --value=1
    expect-equals 1 b.get
    // Closing releases the pin's drive as well as its number.
    a.close
    expect-equals 0 b.get
    a = gpio.Pin A --output --value=1
    expect-equals 1 b.get
    a.close
  finally:
    b.close
