// Toit's standard gpio library on a real board (ztoit#62): two free pins
// joined by a jumper. For the ESP32-C6 devkit: GPIO2 to GPIO11 (as jumpered on the bench, 2026-11-27); GPIO9 is in
// the board's reserved-pins and GPIO30 is past the controller's last pin.
// Prints "GPIO-WIRE PASS" at the end; a failed expectation throws.
import gpio
import expect show *

A ::= 2
B ::= 11
RESERVED ::= 9
NO-PIN ::= 30

main:
  print "GPIO-WIRE start"
  expect-equals "INVALID_ARGUMENT" (catch: gpio.Pin NO-PIN --input)
  expect-equals "RESTRICTED_PIN" (catch: gpio.Pin RESERVED --input)
  2.repeat:
    out-num := it == 0 ? A : B
    in-num := it == 0 ? B : A
    test-in-out out-num in-num
    test-pulls out-num in-num
    test-open-drain out-num in-num
    test-wait-for out-num in-num
  print "GPIO-WIRE PASS"

test-in-out out-num/int in-num/int:
  out := gpio.Pin out-num --output
  in := gpio.Pin in-num --input
  expect-equals "ALREADY_IN_USE" (catch: gpio.Pin out-num --input)
  10.repeat:
    out.set 1
    expect-equals 1 in.get
    out.set 0
    expect-equals 0 in.get
  out.configure --output --value=1
  expect-equals 1 in.get
  out.close
  in.close
  print "in-out $out-num->$in-num ok"

// Nothing drives the wire: the input's own pull decides its level.
test-pulls out-num/int in-num/int:
  floating := gpio.Pin out-num --input
  in := gpio.Pin in-num --input --pull-up
  sleep --ms=2
  expect-equals 1 in.get
  in.set-pull --down
  sleep --ms=2
  expect-equals 0 in.get
  in.set-pull --up
  sleep --ms=2
  expect-equals 1 in.get
  in.configure --input --pull-down
  sleep --ms=2
  expect-equals 0 in.get
  floating.close
  in.close
  print "pulls on $in-num ok"

// An open-drain output only pulls low: released, the input's pull-up wins.
test-open-drain out-num/int in-num/int:
  out := gpio.Pin out-num --output --open-drain --value=1
  in := gpio.Pin in-num --input --pull-up
  sleep --ms=2
  expect-equals 1 in.get
  out.set 0
  expect-equals 0 in.get
  out.set 1
  sleep --ms=2
  expect-equals 1 in.get
  out.set-open-drain false
  in.set-pull --down
  expect-equals 1 in.get  // Push-pull now drives high against the pull-down.
  out.close
  in.close
  print "open-drain $out-num->$in-num ok"

test-wait-for out-num/int in-num/int:
  out := gpio.Pin out-num --output --value=0
  in := gpio.Pin in-num --input
  in.wait-for 0  // Already there: returns at once.
  5.repeat:
    task:: sleep --ms=50; out.set 1
    with-timeout --ms=2000: in.wait-for 1
    expect-equals 1 in.get
    task:: sleep --ms=50; out.set 0
    with-timeout --ms=2000: in.wait-for 0
    expect-equals 0 in.get
  expect-equals "DEADLINE_EXCEEDED" (catch: with-timeout --ms=200: in.wait-for 1)
  out.close
  in.close
  // Closed pins can be taken again.
  again := gpio.Pin in-num --input
  again.close
  print "wait-for $out-num->$in-num ok"
