// ztoit Thread smoke test: UDP over IPv6 from Toit to the gateway's udp-echo6
// (gw85224-01, :7777, reflects byte-verbatim), through the ordinary net API.
// Build with the Thread layer (see zephyr/prj_thread.conf).
import net
import net.udp

GATEWAY ::= "fdc5:9109:9afe:97ea:9541:d963:f8d9:8567"  // Mesh-local EID.
PORT ::= 7777
ROUNDS ::= 5

main:
  network := net.open
  print "network $network.name, address $network.address"
  target := net.SocketAddress (network.resolve GATEWAY)[0] PORT
  socket := network.udp-open
  ok := 0
  ROUNDS.repeat: | i |
    message := "ztoit toit echo $i".to-byte-array
    start := Time.monotonic-us
    socket.send (udp.Datagram message target)
    error := catch:
      with-timeout --ms=3000:
        datagram := socket.receive
        elapsed := (Time.monotonic-us - start) / 1000
        if datagram.data == message:
          ok++
          print "echo $i: $datagram.data.size B in $elapsed ms from $datagram.address"
        else:
          print "echo $i: mismatch ($datagram.data.size B)"
    if error: print "echo $i: $error"
  socket.close
  network.close
  print (ok == ROUNDS ? "UDP ECHO OK $ok/$ROUNDS" : "UDP ECHO FAILED $ok/$ROUNDS")
