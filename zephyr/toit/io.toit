// ztoit#7: Zephyr devices from Toit, by devicetree node label (the C6 overlay).
// Each BOOT press toggles the DO and steps the RGB LED's colour; the BMP180
// prints every 2 s. With no wiring at all, BOOT and the RGB LED still work.
import zephyr

COLOURS ::= [#[16, 0, 0], #[0, 16, 0], #[0, 0, 16], #[0, 0, 0]]

main:
  boot := zephyr.Key "user_button1"
  out := zephyr.Led "do0"
  rgb := zephyr.LedStrip "rgb_led"
  rgb.set COLOURS.last
  print "io: ready"

  task::
    exception := catch:
      bmp := zephyr.Sensor "bmp180"
      while true:
        bmp.fetch
        t := bmp.read zephyr.Sensor.DIE-TEMP  // Zephyr's bmp180 driver reports temperature here.
        p := bmp.read zephyr.Sensor.PRESS
        print "bmp180: $(%.2f t) C $(%.3f p) kPa"
        sleep --ms=2_000
    if exception: print "bmp180: $exception"

  on := false
  while true:
    boot.wait-for-press
    on = not on
    out.set on
    rgb.set COLOURS[(boot.presses - 1) % COLOURS.size]
    print "boot: press $boot.presses, do0 $(on ? "on" : "off")"
