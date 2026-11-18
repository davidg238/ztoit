// ztoit Plan 2 test container: the radio follows net.open/close. Every CYCLE-MS
// it times net.open, does one UDP echo to the gateway (udp-echo6, as soak.toit),
// and closes the network, so the provider takes Thread down between cycles.
import net
import net.udp

GATEWAY ::= "fdc5:9109:9afe:97ea:9541:d963:f8d9:8567"  // Mesh-local EID.
PORT ::= 7777
TIMEOUT-MS ::= 3_000
CYCLE-MS ::= 60_000

echo network/net.Client -> int?:
  socket := network.udp-open
  try:
    message := "net_cycle".to-byte-array
    start := Time.monotonic-us
    socket.send (udp.Datagram message (net.SocketAddress (net.IpAddress.parse GATEWAY) PORT))
    reply := with-timeout --ms=TIMEOUT-MS: socket.receive
    if reply.data != message: return null
    return (Time.monotonic-us - start) / 1_000
  finally:
    socket.close

main:
  while true:
    start := Time.monotonic-us
    network/net.Client? := null
    // A failed open (WiFi connect timeout, Thread attach) is retried next cycle:
    // this container is what opens the link for the agent's check-ins (#35).
    open-error := catch: network = net.open
    if not network:
      print "net_cycle: open failed after $((Time.monotonic-us - start) / 1_000) ms: $open-error"
      sleep --ms=CYCLE-MS
      continue
    print "net_cycle: open $((Time.monotonic-us - start) / 1_000) ms"
    try:
      ms/int? := null
      catch: ms = echo network
      if ms: print "net_cycle: echo ok $ms ms"
      else: print "net_cycle: echo failed"
    finally:
      network.close
    sleep --ms=CYCLE-MS
