// ztoit Thread smoke test: TCP over IPv6 from Toit to the gateway's tcp-echo6
// (gw85224-01, :7777, tools/gateway/), through the ordinary net API.
// Build with the Thread layer (see zephyr/prj_thread.conf).
import net

GATEWAY ::= "fdc5:9109:9afe:97ea:9541:d963:f8d9:8567"  // Mesh-local EID.
PORT ::= 7777

// Reads exactly `size` bytes, or throws.
read-exactly socket size/int -> ByteArray:
  result := ByteArray 0
  while result.size < size:
    chunk := socket.in.read
    if not chunk: throw "closed after $result.size of $size B"
    result += chunk
  return result

main:
  network := net.open
  start := Time.monotonic-us
  socket := network.tcp-connect GATEWAY PORT
  print "connected in $((Time.monotonic-us - start) / 1000) ms: $socket.local-address -> $socket.peer-address"
  ok := 0
  // Small messages, then ones that span several 802.15.4 frames and TCP segments.
  [17, 100, 1000, 4000].do: | size |
    message := ByteArray size: it & 0xff
    start = Time.monotonic-us
    // Write from a task while reading here: the echo server stops reading once
    // our receive window is full, so writing everything first would deadlock.
    task:: socket.out.write message
    reply := read-exactly socket size
    elapsed := (Time.monotonic-us - start) / 1000
    if reply == message:
      ok++
      print "echo $size B in $elapsed ms"
    else:
      print "echo $size B: mismatch"
  socket.close
  network.close
  print (ok == 4 ? "TCP ECHO OK $ok/4" : "TCP ECHO FAILED $ok/4")
