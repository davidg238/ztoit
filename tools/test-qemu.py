#!/usr/bin/env python3
"""Run Toit's core tests on the Zephyr port under QEMU (ztoit#61).

    tools/test-qemu.py [-o results.txt] [-d build-dir] [test ...]

A test is named by its path under the fork's tests/ without .toit
(`block-test`, `regress/issue17-test`), or ztoit's own tests/qemu/*-test.toit
as `ztoit/<name>` (`ztoit/gpio-test`, the port's devices under the board's
emulators); none named runs every upstream test that toit.run starts without
flags: tests/*-test.toit, *-test-slow.toit and the same in regress/
(tests/CMakeLists.txt; -force, -no-boot and -compiler tests need toit.run
options or the host compiler), then ztoit's.

Each test is compiled here, unmodified, to a relocatable image; a firmware
for qemu_riscv32 holds BATCH of them with tests/qemu/runner.toit, which starts
them one at a time and prints `TEST <n> START` and then `TEST <n> PASS|FAIL|TIMEOUT`.
If the VM dies or stops answering inside a test, that test is recorded and the
firmware is rebuilt with the tests not yet run.

Writes the results file (`name PASS|FAIL|TIMEOUT|CRASH|SKIP reason`, then the
counts) and console.log in the build directory. Source env.sh first.
"""
import argparse
import collections
import concurrent.futures
import os
import re
import shutil
import subprocess
import sys
import time

ZTOIT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ZTOIT, "build-t61-qemu")  # -d replaces it
WORK = os.path.join(BUILD, "work")       # the copied tests/ and the images
RUNNER = os.path.join(ZTOIT, "tests", "qemu", "runner.toit")
OWN = os.path.join(ZTOIT, "tests", "qemu")  # ztoit's tests, run as ztoit/<name>
# Real seconds a test may run before the VM counts as hung. The runner's own
# timeout (runner.toit's TIMEOUT, virtual time) normally fires well before.
HUNG_S = 300
BOOT_S = 120   # real seconds from QEMU start to the first test
# Tests per firmware. The runner lists the images with containers.images,
# whose reply is one list of 4 entries per image; a list longer than
# ARRAYLET-SIZE (500, lib/core/collections.toit) is a large array, which the
# message encoder refuses, so a boot holds at most 125 images with the system
# program and the runner.
BATCH = 100


def env(name):
    value = os.environ.get(name)
    if not value:
        sys.exit(f"{name} is not set: source env.sh")
    return value


def all_tests(tests_dir):
    names = []
    for sub in ("", "regress"):
        for f in sorted(os.listdir(os.path.join(tests_dir, sub))):
            if re.search(r"-test(-slow)?\.toit$", f):
                names.append(os.path.join(sub, f[:-len(".toit")]))
    for f in sorted(os.listdir(OWN)):
        if re.search(r"-test\.toit$", f):
            names.append("ztoit/" + f[:-len(".toit")])
    return names


def copy_tests(tests_dir):
    """The fork's tests/ without its package.lock: that lock names packages
    (host, cli...) that are not installed, and the compiler then refuses every
    file below it. Tests that import a package fail to compile, as SKIP."""
    shutil.rmtree(WORK, ignore_errors=True)
    shutil.copytree(tests_dir, os.path.join(WORK, "tests"), symlinks=True)
    for f in ("package.lock", "package.yaml"):
        os.remove(os.path.join(WORK, "tests", f))
    own = os.path.join(WORK, "tests", "ztoit")
    os.makedirs(own)
    for f in os.listdir(OWN):
        if f.endswith(".toit") and f != "runner.toit":
            shutil.copy(os.path.join(OWN, f), own)


def compile_test(toit, name):
    """Answers (name, image or None, snapshot, the compiler's first error)."""
    source = os.path.join(WORK, "tests", name + ".toit")
    out = os.path.join(WORK, "img", name)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    for cmd in ([toit, "compile", "--snapshot", "-o", out + ".snapshot", source],
                [toit, "tool", "snapshot-to-image", "--format=binary", "-m32",
                 "-o", out + ".image", out + ".snapshot"]):
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            lines = (r.stdout + r.stderr).splitlines()
            error = next((l for l in lines if "error:" in l), lines[0] if lines else "?")
            return name, None, None, error.replace(WORK + "/tests/", "").strip()
    return name, out + ".image", out + ".snapshot", None


def build(images, log):
    cmd = ["west", "build", "-b", "qemu_riscv32", "-d", BUILD, os.path.join(ZTOIT, "zephyr"), "--",
           f"-DCONTAINERS={RUNNER}", "-DTESTS=" + ";".join(images)]
    t = time.time()
    r = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT)
    if r.returncode != 0:
        sys.exit(f"build failed: see {log.name}")
    print(f"built {len(images)} test images in {time.time() - t:.0f} s", flush=True)


def qemu_command():
    # Zephyr's own `run` target: the board's QEMU binary and options.
    out = subprocess.run(["ninja", "-C", BUILD, "-t", "commands", "run"],
                         capture_output=True, text=True, check=True).stdout
    return out.strip().splitlines()[-1]


def decode(toit, snapshot, message):
    """A trace from the test, or from the system program on its behalf (a
    service call that threw there, such as net.open)."""
    for s in (snapshot, os.path.join(BUILD, "toit", "system.snapshot")):
        r = subprocess.run([toit, "decode", "--force-plain", "-s", s, message],
                           capture_output=True, text=True)
        out = r.stdout + r.stderr
        if "unexpected snapshot" not in out:
            break
    return [l for l in out.splitlines() if l.strip()]


def boot(batch, snapshots, toit, log, results):
    """One QEMU boot over batch (names, in tag order). Answers the names not
    run: those after a test that hung or crashed the VM."""
    proc = subprocess.Popen(qemu_command(), shell=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL, start_new_session=True)
    os.set_blocking(proc.stdout.fileno(), False)
    current = None      # tag of the test running
    output = []         # its console lines
    tail = collections.deque(maxlen=3)  # the last console lines, for a crash outside a test
    deadline = time.time() + BOOT_S
    pending = b""
    done = False
    try:
        while not done:
            if time.time() > deadline:
                why = f"no result in {HUNG_S} s real time" if current else "no first test"
                break
            chunk = proc.stdout.read()
            if chunk is None:
                time.sleep(0.05)
                continue
            if chunk == b"":
                why = "QEMU exited"
                break
            pending += chunk
            *lines, pending = pending.split(b"\n")
            for raw in lines:
                line = raw.decode("utf-8", "replace").rstrip("\r")
                log.write(line + "\n")
                log.flush()
                tail.append(line)
                m = re.fullmatch(r"TEST (\d+) (START|PASS|FAIL|TIMEOUT)(.*)", line)
                if line == "TESTS DONE":
                    done = True
                elif m and m[2] == "START":
                    current, output = int(m[1]), []
                    deadline = time.time() + HUNG_S
                elif m:
                    name = batch[int(m[1]) - 1]
                    reason = m[3].strip()
                    if m[2] == "TIMEOUT":
                        reason = "runner's timeout (tests/qemu/runner.toit)"
                    detail = output
                    if m[2] == "FAIL":
                        detail = []
                        for o in output:
                            if o.startswith("jag decode "):
                                detail += decode(toit, snapshots[name], o.split()[2])
                            elif not o.startswith("----") and "system message" not in o and o != "make it human readable:":
                                detail.append(o)
                    results[name] = (m[2], reason, detail)
                    print(f"{m[2]:7} {name}", flush=True)
                    current = None
                elif current is not None:
                    output.append(line)
    finally:
        os.killpg(proc.pid, 9)
        proc.wait()
    if done:
        return []
    left = [n for n in batch if n not in results]
    if current is None:
        # The VM died between tests: the rest get a boot of their own, unless
        # this boot ran none, when they never will.
        if len(left) < len(batch):
            return left
        for name in left:
            results[name] = ("CRASH", why, list(tail))
        return []
    name = batch[current - 1]
    kind = "TIMEOUT" if why.startswith("no result") else "CRASH"
    results[name] = (kind, why, output[-20:])
    print(f"{kind:7} {name}", flush=True)
    return batch[current:]


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("-o", "--output")
    parser.add_argument("-d", "--build-dir", help="the build directory (default build-t61-qemu)")
    parser.add_argument("tests", nargs="*")
    args = parser.parse_args()
    global BUILD, WORK
    if args.build_dir:
        BUILD = os.path.abspath(args.build_dir)
        WORK = os.path.join(BUILD, "work")
    if not args.output:
        args.output = os.path.join(BUILD, "results.txt")
    toit = env("TOIT_HOST")
    fork = env("TOIT_SRC")
    tests_dir = os.path.join(fork, "tests")
    names = args.tests or all_tests(tests_dir)
    start = time.time()

    os.makedirs(BUILD, exist_ok=True)
    copy_tests(tests_dir)
    results = {}
    images, snapshots = {}, {}
    with concurrent.futures.ThreadPoolExecutor(os.cpu_count()) as pool:
        for name, image, snapshot, error in pool.map(lambda n: compile_test(toit, n), names):
            if image:
                images[name], snapshots[name] = image, snapshot
            else:
                results[name] = ("SKIP", "does not compile: " + error, [])
    print(f"compiled {len(images)} of {len(names)} tests in {time.time() - start:.0f} s", flush=True)

    # Tests that compile to the same program get the same image id, and the
    # registry lists an id once: such a test runs once, its twins share the result.
    first = {}  # image bytes -> the first test with them
    for n in names:
        if n in images:
            with open(images[n], "rb") as f:
                first.setdefault(f.read(), n)
    twin = {n: first[open(images[n], "rb").read()] for n in images}
    queue = [n for n in names if twin.get(n) == n]
    with open(os.path.join(BUILD, "build.log"), "w") as build_log, \
         open(os.path.join(BUILD, "console.log"), "w") as log:
        while queue:
            batch, queue = queue[:BATCH], queue[BATCH:]
            build([images[n] for n in batch], build_log)
            left = boot(batch, snapshots, toit, log, results)
            for n in batch:  # neither run nor left for the next boot: a bug in the runner or here
                if n not in left:
                    results.setdefault(n, ("CRASH", "the runner did not report it", []))
            queue = left + queue
    for n, t in twin.items():
        if n != t:
            kind, reason, detail = results[t]
            results[n] = (kind, f"same program as {t}: {reason}".rstrip(": "), detail)

    counts = collections.Counter(r[0] for r in results.values())
    summary = " ".join(f"{k} {counts[k]}" for k in ("PASS", "FAIL", "TIMEOUT", "CRASH", "SKIP"))
    with open(args.output, "w") as out:
        for name in names:
            kind, reason, detail = results[name]
            out.write(f"{name} {kind} {reason}".rstrip() + "\n")
            if kind != "PASS":
                for d in detail[:12]:
                    # tests/<file> and system/<file>, not this machine's paths
                    out.write(f"    {d.replace(WORK + '/', '').replace(fork + '/', '')}\n")
        out.write(f"# {summary} of {len(names)}, {time.time() - start:.0f} s\n")
    print(f"{summary} of {len(names)} in {time.time() - start:.0f} s: {args.output}")


if __name__ == "__main__":
    main()
