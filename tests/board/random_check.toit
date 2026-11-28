// crypto.random on a real board (ztoit#68): prints 16 random bytes twice and a
// UUID. Two calls must differ, and the lines must differ from boot to boot.
import crypto show random
import uuid

main:
  a := random --size=16
  b := random --size=16
  if a == b: throw "RANDOM-CHECK: two calls returned the same bytes"
  if (a.every: it == a[0]): throw "RANDOM-CHECK: constant bytes"
  print "RANDOM-CHECK $(hex a) $(hex b) $(uuid.Uuid.random)"

hex bytes/ByteArray -> string:
  return (List bytes.size: "$(%02x bytes[it])").join ""
