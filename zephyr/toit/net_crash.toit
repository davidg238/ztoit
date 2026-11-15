// ztoit Plan 2 test container: dies holding the network. The service manager
// must release its network resource, or Thread never goes down again.
import net

main:
  sleep --ms=10_000
  network := net.open
  print "net_crash: open"
  throw "net_crash: on purpose"
