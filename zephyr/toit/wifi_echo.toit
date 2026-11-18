// ztoit WiFi test container (rete #521): the station follows net.open/close.
// Every CYCLE-MS it times net.open, does one UDP echo to the gateway's LAN
// address (an IPv4 peer, v4-mapped by the VM's IPv6 sockets), and closes.
import net
import net.udp

GATEWAY ::= "192.168.0.176"  // gw85224-01, enp4s0.
PORT ::= 7777
TIMEOUT-MS ::= 3_000
CYCLE-MS ::= 30_000

echo network/net.Client -> int?:
  socket := network.udp-open
  try:
    message := "wifi_echo".to-byte-array
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
      print "wifi_echo: open failed after $((Time.monotonic-us - start) / 1_000) ms: $open-error"
      sleep --ms=CYCLE-MS
      continue
    print "wifi_echo: open $((Time.monotonic-us - start) / 1_000) ms, address $network.address"
    try:
      ms/int? := null
      error := catch: ms = echo network
      if ms: print "wifi_echo: echo ok $ms ms"
      else: print "wifi_echo: echo failed $error"
    finally:
      network.close
    sleep --ms=CYCLE-MS
