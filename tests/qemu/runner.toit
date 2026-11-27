// The QEMU test runner (ztoit#61): starts each test image of the firmware
// (zephyr/CMakeLists.txt's $TESTS, tagged 1..n in the image's data) one at a
// time, as a container of its own, and prints one line per test for
// tools/test-qemu.py, which maps the numbers back to file names.
// A test passes when its container exits with 0: an uncaught exception or a
// non-zero exit fails it.
import system.containers

// As upstream: tests/CMakeLists.txt runs each test as `toit.run <file> foo bar
// gee` under a 40 s timeout. Here it is QEMU's virtual time (-icount).
ARGUMENTS ::= ["foo", "bar", "gee"]
TIMEOUT ::= Duration --s=40

main:
  tests := containers.images.filter: | image/containers.ContainerImage | image.data > 0
  tests.sort --in-place: | a/containers.ContainerImage b/containers.ContainerImage | a.data - b.data
  tests.do: | image/containers.ContainerImage |
    n := image.data
    print "TEST $n START"
    container := containers.start image.id ARGUMENTS
    code/int? := null
    catch --unwind=(: it != DEADLINE-EXCEEDED-ERROR):
      with-timeout TIMEOUT: code = container.wait
    if code == null:
      container.stop
      print "TEST $n TIMEOUT"
    else if code == 0:
      print "TEST $n PASS"
    else:
      print "TEST $n FAIL exit $code"
  print "TESTS DONE"
