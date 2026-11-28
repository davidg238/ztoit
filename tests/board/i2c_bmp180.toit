// Toit's standard i2c library on a real board (ztoit#62): the ESP32-C6
// devkit's i2c0 (SDA GPIO6, SCL GPIO7, the overlay's ztoit,buses node) with a
// BMP180 breakout at 0x77. Reads the chip id (register 0xD0, 0x55) and the
// calibration block (0xAA..0xBF, eleven big-endian words, none 0 or 0xFFFF:
// the datasheet's validity rule), at 100 and 400 kHz. The same controller is
// the devicetree `bmp180` sensor's: ztoit.Sensor reads it before, while and
// after the bus is open. Prints "I2C-BMP180 PASS" at the end; a failed
// expectation throws.
import gpio
import i2c
import expect show *
import ztoit

SDA ::= 6
SCL ::= 7
BMP180 ::= 0x77

main:
  print "I2C-BMP180 start"
  sensor "before"
  expect-equals "INVALID_ARGUMENT" (catch: i2c.Bus --sda=SCL --scl=SDA)
  expect-equals "INVALID_ARGUMENT" (catch: i2c.Bus --sda=2 --scl=11)
  bus := i2c.Bus --sda=SDA --scl=SCL --frequency=100_000
  expect-equals "ALREADY_IN_USE" (catch: i2c.Bus --sda=SDA --scl=SCL)
  expect-equals "RESTRICTED_PIN" (catch: gpio.Pin SDA --input)
  expect-equals "ALREADY_IN_USE" (catch: gpio.Pin SDA --allow-restricted --input)
  found := bus.scan
  print "scan: $(found.map: "0x$(%02x it)")"
  absent := 0x08
  while found.contains absent: absent++
  expect-not (bus.test absent)
  expect-equals "I2C_NACK" (catch: (bus.device absent).read 1)
  print "nack at 0x$(%02x absent) ok"
  expect (found.contains BMP180)
  check (bus.device BMP180) "100 kHz"
  check (bus.device BMP180 --frequency=400_000) "400 kHz"
  sensor "while open"
  bus.close
  sensor "after close"
  3.repeat:
    bus = i2c.Bus --sda=SDA --scl=SCL
    expect (bus.test BMP180)
    bus.close
  print "reopen ok"
  pin-objects
  print "I2C-BMP180 PASS"

// A bus takes pin numbers only; a gpio.Pin is refused before the pins are
// matched. No Pin is made on a bus pin: making and closing one disconnects
// it, and the controller then stays dead until reboot. Two free pins stand
// in; the Pins are closed before anything else runs.
pin-objects:
  a := gpio.Pin 2 --input
  b := gpio.Pin 11 --input
  error := catch: i2c.Bus --sda=a --scl=b
  a.close
  b.close
  expect-equals "INVALID_ARGUMENT" error
  print "pin objects refused ok"

check device/i2c.Device label/string:
  registers := device.registers
  id := registers.read-u8 0xD0
  print "$label: chip id 0x$(%02x id)"
  expect-equals 0x55 id
  calibration := registers.read-bytes 0xAA 22
  words := List 11: (calibration[2 * it] << 8) | calibration[2 * it + 1]
  print "$label: calibration $words"
  words.do: expect (it != 0 and it != 0xFFFF)
  device.close

// Reports, does not fail: whether the devicetree driver still works.
sensor label/string:
  error := catch:
    bmp := ztoit.Sensor "bmp180"
    bmp.fetch
    t := bmp.read ztoit.Sensor.DIE-TEMP
    p := bmp.read ztoit.Sensor.PRESS
    print "sensor $label: $(%.2f t) C $(%.3f p) kPa"
  if error: print "sensor $label: $error"
