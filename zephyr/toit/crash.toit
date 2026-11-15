// A container that throws: proves one container's crash leaves the others running.
main:
  print "crash: start"
  sleep --ms=2_000
  throw "crash: on purpose"
