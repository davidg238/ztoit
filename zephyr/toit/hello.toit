// ztoit#1: the Step 0 baseline program, run as the Zephyr boot program.
// Stock Toit v2.0.0 on ESP32-C6 printed the numbers in spike/baseline/README.md
// (100 tasks there; 8 here, see ztoit#3).
import system

report label/string:
  s := system.process-stats --gc
  print "$label: process allocated=$s[system.STATS-INDEX-ALLOCATED-MEMORY] reserved=$s[system.STATS-INDEX-RESERVED-MEMORY] system free=$s[system.STATS-INDEX-SYSTEM-FREE-MEMORY] largest=$s[system.STATS-INDEX-SYSTEM-LARGEST-FREE]"

main:
  print "hello"
  report "idle"
  started := 0
  8.repeat:
    task::
      started++
      sleep --ms=3_600_000
  sleep --ms=500
  print "tasks started=$started"
  report "8 tasks"
  n := 0
  while true:
    sleep --ms=1000
    n++
    if n % 5 == 0: report "tick $n"
