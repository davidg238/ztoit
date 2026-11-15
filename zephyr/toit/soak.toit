// ztoit Thread soak (ztoit#4): UDP and TCP echoes to gw85224-01 (:7777, udp-echo6
// and tcp-echo6) every ROUND-MS, forever. Each failure prints at once; every
// SUMMARY-ROUNDS rounds a summary line prints counts, round-trip times and the
// process heap, so leaks and drift show over hours. One persistent TCP
// connection, re-opened after any failure.
import net
import net.udp
import net.tcp
import system

GATEWAY ::= "fdc5:9109:9afe:97ea:9541:d963:f8d9:8567"  // Mesh-local EID.
PORT ::= 7777
ROUND-MS ::= 5_000
TIMEOUT-MS ::= 3_000
SUMMARY-ROUNDS ::= 37  // ztoit#9: off the 300 s period, to separate the summary from 300 s timers.
UDP-SIZE ::= 64
TCP-SIZE ::= 256

class Stats:
  name/string
  ok := 0
  failed := 0
  min-ms := 1_000_000
  max-ms := 0
  total-ms := 0
  window-ok := 0  // Echoes since the last summary: the window min/mean/max cover.

  constructor .name:

  record ms/int:
    ok++
    window-ok++
    total-ms += ms
    min-ms = min min-ms ms
    max-ms = max max-ms ms

  stringify -> string:
    mean := window-ok == 0 ? 0 : total-ms / window-ok
    return "$name ok=$ok failed=$failed window rtt min/mean/max=$(window-ok == 0 ? 0 : min-ms)/$mean/$max-ms ms"

  reset-window -> none:
    window-ok = 0
    min-ms = 1_000_000
    max-ms = 0
    total-ms = 0

read-exactly socket/tcp.Socket size/int -> ByteArray:
  result := ByteArray 0
  while result.size < size:
    chunk := socket.in.read
    if not chunk: throw "closed after $result.size of $size B"
    result += chunk
  return result

// Fills `bytes` with a pattern derived from `seq`, so stale echoes don't match.
payload size/int seq/int -> ByteArray:
  return ByteArray size: (seq + it * 7) & 0xff

main:
  network := net.open
  print "soak: start, address $network.address, round $ROUND-MS ms"
  target := net.SocketAddress (net.IpAddress.parse GATEWAY) PORT
  udp-socket := network.udp-open
  tcp-socket/tcp.Socket? := null
  udp-stats := Stats "udp"
  tcp-stats := Stats "tcp"
  tcp-connects := 0
  round := 0
  lost/ByteArray? := null  // ztoit#9: the last unanswered UDP message, to report a late echo.
  lost-us := 0
  while true:
    round++
    round-start := Time.monotonic-us
    start := round-start

    // UDP: one datagram, wait for its echo; drop stale echoes from late rounds.
    message := payload UDP-SIZE round
    error := catch:
      udp-socket.send (udp.Datagram message target)
      with-timeout --ms=TIMEOUT-MS:
        while true:
          datagram := udp-socket.receive
          if datagram.data == message: break
          if lost and datagram.data == lost:
            print "soak: late udp echo $((Time.monotonic-us - lost-us) / 1000) ms after send"
            lost = null
      udp-stats.record (Time.monotonic-us - start) / 1000
    if error:
      udp-stats.failed++
      lost = message
      lost-us = start
      print "soak: round $round udp FAILED at uptime $(start / 1000) ms: $error"

    // TCP: one message on the persistent connection, re-opened after a failure.
    start = Time.monotonic-us
    message = payload TCP-SIZE round
    error = catch:
      if not tcp-socket:
        tcp-socket = network.tcp-connect target
        tcp-connects++
      with-timeout --ms=TIMEOUT-MS:
        tcp-socket.out.write message
        reply := read-exactly tcp-socket TCP-SIZE
        if reply != message: throw "mismatch"
      tcp-stats.record (Time.monotonic-us - start) / 1000
    if error:
      tcp-stats.failed++
      print "soak: round $round tcp FAILED: $error"
      if tcp-socket:
        catch: tcp-socket.close
        tcp-socket = null

    if round % SUMMARY-ROUNDS == 0:
      stats := system.process-stats --gc
      print "soak: round $round uptime $(Time.monotonic-us / 1_000_000) s | $udp-stats | $tcp-stats connects=$tcp-connects | heap allocated=$stats[system.STATS-INDEX-ALLOCATED-MEMORY] reserved=$stats[system.STATS-INDEX-RESERVED-MEMORY]"
      udp-stats.reset-window
      tcp-stats.reset-window

    elapsed-ms := (Time.monotonic-us - round-start) / 1000
    sleep --ms=(max 0 (ROUND-MS - elapsed-ms))
