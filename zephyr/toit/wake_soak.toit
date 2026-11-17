// ztoit battery soak: every CYCLE-MS open the network, do one UDP echo to the
// gateway (udp-echo6, as soak.toit), print one report line with running counts,
// and close the network, so the provider takes Thread down and the node sleeps
// between cycles. For a battery-powered board left overnight.
import net
import net.udp

GATEWAY ::= "fdc5:9109:9afe:97ea:9541:d963:f8d9:8567"  // Mesh-local EID.
PORT ::= 7777
TIMEOUT-MS ::= 3_000
CYCLE-MS ::= 300_000

echo network/net.Client cycle/int -> int:
  socket := network.udp-open
  try:
    message := "wake_soak $cycle".to-byte-array
    start := Time.monotonic-us
    socket.send (udp.Datagram message (net.SocketAddress (net.IpAddress.parse GATEWAY) PORT))
    with-timeout --ms=TIMEOUT-MS:
      while (socket.receive).data != message: null  // Skip a late echo of an earlier cycle.
    return (Time.monotonic-us - start) / 1_000
  finally:
    socket.close

main:
  ok := 0
  failed := 0
  cycle := 0
  while true:
    cycle++
    start := Time.monotonic-us
    result := "open failed"
    error := catch:
      network := net.open
      open-ms := (Time.monotonic-us - start) / 1_000
      try:
        result = "open $open-ms ms, echo failed"
        result = "open $open-ms ms, echo $(echo network cycle) ms"
      finally:
        network.close
    if error: failed++ else: ok++
    print "wake_soak: cycle $cycle uptime $(start / 1_000_000) s: $result$(error ? " ($error)" : "") | ok=$ok failed=$failed"
    sleep --ms=CYCLE-MS
