// #37 camera check under stock Toit (Jaguar): reads registers 0x44 and 0x40-0x43.
// `jag run -d camera spike/arducam/jag_probe.toit`; 0xff everywhere means no answer.
import gpio
import spi

main:
  bus := spi.Bus --mosi=(gpio.Pin 23) --miso=(gpio.Pin 19) --clock=(gpio.Pin 18)
  cam := bus.device --cs=(gpio.Pin 22) --frequency=100_000 --mode=0
  3.repeat: | round |
    line := "jagprobe $round:"
    [0x44, 0x40, 0x41, 0x42, 0x43].do: | reg |
      data := #[reg & 0x7f, 0, 0]
      cam.transfer data --read
      line += " $(%02x reg)=$(%02x data[0]) $(%02x data[1]) $(%02x data[2])"
    print line
    sleep --ms=500
