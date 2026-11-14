// ztoit Thread smoke test: UDP over IPv6 from Toit to the gateway's udp-echo6
// (gw85224-01, :7777, reflects byte-verbatim). Build with the Thread layer:
//   west build -- -DPROGRAM=.../udp_echo.toit -DEXTRA_CONF_FILE=... (see prj_thread.conf)
// Uses net.modules.udp directly: there is no network service in ztoit's boot
// program yet, so `net.open` is not available.
import net
import net.udp
import net.modules.udp as udp-module

GATEWAY ::= "fdc5:9109:9afe:97ea:9541:d963:f8d9:8567"  // Mesh-local EID.
PORT ::= 7777
ROUNDS ::= 5

// The socket only needs an Interface to resolve its bind address, which is a
// literal here, so nothing in this class is ever called.
class Thread_ implements udp.Interface:
  udp-open -> udp.Socket: return udp-open --port=null
  udp-open --port/int? -> udp.Socket:
    return udp-module.Socket this "0.0.0.0" (port or 0)

// Parses a full (uncompressed) IPv6 address.
ipv6 text/string -> net.IpAddress:
  groups := text.split ":"
  if groups.size != 8: throw "need 8 groups: $text"
  raw := ByteArray 16
  groups.size.repeat: | i |
    value := int.parse groups[i] --radix=16
    raw[2 * i] = value >> 8
    raw[2 * i + 1] = value & 0xff
  return net.IpAddress raw

main:
  socket := (Thread_).udp-open
  target := net.SocketAddress (ipv6 GATEWAY) PORT
  ok := 0
  ROUNDS.repeat: | i |
    message := "ztoit toit echo $i".to-byte-array
    start := Time.monotonic-us
    socket.send (udp.Datagram message target)
    reply := catch:
      with-timeout --ms=3000:
        datagram := socket.receive
        elapsed := (Time.monotonic-us - start) / 1000
        if datagram.data == message:
          ok++
          print "echo $i: $datagram.data.size B in $elapsed ms from $datagram.address"
        else:
          print "echo $i: mismatch ($datagram.data.size B)"
    if reply: print "echo $i: $reply"
  socket.close
  print (ok == ROUNDS ? "UDP ECHO OK $ok/$ROUNDS" : "UDP ECHO FAILED $ok/$ROUNDS")
