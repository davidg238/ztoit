// Toit's standard i2c library on the port (ztoit#62), controller role, under
// QEMU with zephyr/boards/qemu_riscv32.overlay: an emulated I2C controller
// that the overlay's ztoit,buses node gives pins 4 (sda) and 5 (scl), both in
// reserved-pins, with Zephyr's AT24 EEPROM emulator at 0x50 (256 bytes, a
// register pointer, erased to 0xff). Written against `i2c` (and `gpio` for the
// pins) only.
//
// The emulator refuses any speed above 100 kHz with -EIO, so a 400 kHz
// device reads as a NACK here: that shows the speed reaches the controller.
import gpio
import i2c
import expect show *

SDA ::= 4
SCL ::= 5
EEPROM ::= 0x50
ABSENT ::= 0x51

main:
  test-pins
  test-registers
  test-read-write
  test-probe
  test-absent
  test-device-options
  test-pin-objects
  test-target
  test-close-reopen

open -> i2c.Bus:
  return i2c.Bus --sda=SDA --scl=SCL --frequency=100_000

test-pins:
  // Only the pair the board gave a controller opens it.
  expect-equals "INVALID_ARGUMENT" (catch: i2c.Bus --sda=SCL --scl=SDA)
  expect-equals "INVALID_ARGUMENT" (catch: i2c.Bus --sda=SDA --scl=6)
  expect-equals "INVALID_ARGUMENT" (catch: i2c.Bus --sda=0 --scl=1)
  bus := open
  try:
    // One open bus per controller.
    expect-equals "ALREADY_IN_USE" (catch: open)
    // The bus holds its pins, also against --allow-restricted.
    expect-equals "ALREADY_IN_USE" (catch: gpio.Pin SDA --allow-restricted)
    expect-equals "ALREADY_IN_USE" (catch: gpio.Pin SCL --allow-restricted --input)
    // Without the flag a bus pin is restricted, as on a board.
    expect-equals "RESTRICTED_PIN" (catch: gpio.Pin SDA)
  finally:
    bus.close

test-registers:
  bus := open
  try:
    device := bus.device EEPROM
    registers := device.registers
    registers.write-bytes 0x10 #[1, 2, 3, 4]
    expect-equals #[1, 2, 3, 4] (registers.read-bytes 0x10 4)
    registers.write-u8 0x20 0x5a
    expect-equals 0x5a (registers.read-u8 0x20)
    registers.write-u16-be 0x30 0x1234
    expect-equals 0x1234 (registers.read-u16-be 0x30)
    // Erased bytes read 0xff.
    expect-equals 0xff (registers.read-u8 0x80)
  finally:
    bus.close

test-read-write:
  bus := open
  try:
    device := bus.device EEPROM
    // A plain write: the register pointer, then data.
    device.write #[0x40, 9, 8, 7, 6]
    // A write that only sets the pointer, then a plain read from it.
    device.write #[0x40]
    expect-equals #[9, 8, 7, 6] (device.read 4)
    // The pointer advanced past what was read.
    expect-equals #[0xff] (device.read 1)
    // Write then read with a repeated start.
    expect-equals #[8, 7] (device.write-read #[0x41] 2)
    buffer := ByteArray 3
    device.write-read-into --tx-buffer=#[0x40] --rx-buffer=buffer
    expect-equals #[9, 8, 7] buffer
    // A long transfer, more than one FIFO's worth on real controllers.
    data := ByteArray 64: it
    device.write (#[0x80] + data)
    expect-equals data (device.write-read #[0x80] 64)
  finally:
    bus.close

test-probe:
  bus := open
  try:
    expect (bus.test EEPROM)
    expect-not (bus.test ABSENT)
    expect-equals {EEPROM} bus.scan
  finally:
    bus.close

test-absent:
  bus := open
  try:
    absent := bus.device ABSENT
    expect-equals "I2C_NACK" (catch: absent.write #[0])
    expect-equals "I2C_NACK" (catch: absent.read 1)
    expect-equals "I2C_NACK" (catch: absent.write-read #[0] 1)
    // The bus still works after a NACK.
    expect-equals 0xff ((bus.device EEPROM).registers.read-u8 0xf0)
  finally:
    bus.close

test-device-options:
  bus := open
  try:
    expect-equals "INVALID_ARGUMENT" (catch: bus.device EEPROM --frequency=200_000)
    expect-equals "UNIMPLEMENTED" (catch: bus.device EEPROM --address-bit-size=10)
    expect-equals "UNIMPLEMENTED" (catch: bus.device EEPROM --timeout-us=1000)
    expect-equals "UNIMPLEMENTED" (catch: bus.device EEPROM --disable-ack-check)
    fast := bus.device EEPROM --frequency=400_000
    expect-equals "I2C_NACK" (catch: fast.read 1)  // The emulator's 100 kHz limit.
    fast.close
    slow := bus.device EEPROM --frequency=100_000
    slow.write #[0x00]
    expect-equals 1 (slow.read 1).size
    // A bus's devices close with it, so the bus can close with one open.
  finally:
    bus.close

// The deprecated form: Pins the program opened, with --allow-restricted as a
// bus pin is reserved. The bus uses them and leaves them to their owner.
test-pin-objects:
  sda := gpio.Pin SDA --allow-restricted
  scl := gpio.Pin SCL --allow-restricted
  bus := i2c.Bus --sda=sda --scl=scl --frequency=100_000
  expect (bus.test EEPROM)
  bus.close
  // Still the program's: the bus did not release them.
  expect-equals "ALREADY_IN_USE" (catch: gpio.Pin SDA --allow-restricted)
  sda.close
  scl.close

test-target:
  expect-equals "UNIMPLEMENTED" (catch: i2c.Target --sda=SDA --scl=SCL --address=0x42)

test-close-reopen:
  bus := open
  bus.close
  expect-equals "CLOSED" (catch: bus.test EEPROM)
  // Closed: the pins are free.
  pin := gpio.Pin SDA --allow-restricted
  pin.close
  3.repeat:
    bus = open
    expect (bus.test EEPROM)
    expect-equals 9 ((bus.device EEPROM).registers.read-u8 0x40)
    bus.close
