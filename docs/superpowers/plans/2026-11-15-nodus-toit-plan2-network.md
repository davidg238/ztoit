# nodus-toit Plan 2: the Thread network provider brings the radio up and down

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Thread comes up on a container's first `net.open` and goes "down" after the last close, instead of
`main.cc` attaching before the VM starts.

**Architecture:** The fork's `ZephyrNetworkProvider` adopts the stock `NetworkState` refcount (as the ESP32
`WifiServiceProvider` does): `up` creates a `ThreadModule` whose `connect` starts Thread and polls the role until
attached; `disconnect` runs on the last close. Three new primitives, appended to the fork's `zephyr` module, wrap
OpenThread. "Down" is a per-node build setting (design §7): default a sleepy end device (SED), or fully detached.
ztoit's `thread.cc` and the boot-time echo are deleted.

**Tech Stack:** Zephyr 4.2 + OpenThread MTD, the Toit fork `~/workspaceToit/toit-v2.0.0` branch `zephyr-port`,
fork compiler `$TOIT_HOST` (`tools/fork-sdk.sh`), ztoit CMake.

**Spec:** `docs/design/2026-11-15-nodus-toit-design.md` §5 (network providers), §7 (radio, "down" on Thread).
Step 2 of its §11 order. Plan 1: `docs/superpowers/plans/2026-11-15-nodus-toit-plan1-containers.md`.

## Global Constraints

- Apps stay ordinary Toit programs: `net.open` / `network.close`, no ztoit-specific calls (design §2).
- New primitives are appended **last** in `MODULE_ZEPHYR` (`src/primitive.h`); rebuild the fork compiler with
  `tools/fork-sdk.sh` after changing it (`docs/design/zephyr-port.md`, #7 addendum).
- Fork pushes: `git push git@github.com:davidg238/toit.git HEAD:zephyr-port`, never `origin`. ztoit `west.yml`
  pin bumped in the ztoit commit that needs it.
- **Reflashing a board needs David's explicit go, every time.** C6 `/dev/ttyUSB0`; dongles are owned by the
  #9/#12 soaks.
- Serial logs need `grep -a`. Commits: `refs #10`, body ≤ 5 lines.
- No new numbers: the attach timeout stays thread.cc's 60 s; the SED poll period is derived from OpenThread's
  child timeout (below), not chosen.

## Review Focus

1. **Attach fails** (no gateway, wrong dataset). `net.open` throws `thread: attach timed out` after 60 s, the
   usage count returns to 0 (stock `NetworkState.up` handles it), and the next `net.open` retries. Task 2 step 6
   proves the timeout; the retry rests on the stock code unless David asks for it on hardware.
2. **Overlapping users.** One container holds the network while another opens and closes it: no "down" until
   the last close. Task 2 step 5 (soak + net_cycle).
3. **A container dies holding the network.** Its resource is closed by the service manager, so "down" follows.
   Task 2 step 5 (`net_crash.toit`).
4. **No container opens the network.** The radio is never started: no `thread:` line in the log. Task 2 step 7.
5. **A build without Thread** (`prj_thread.conf` absent). The primitives `FAIL(UNSUPPORTED)`, `net.open` throws,
   containers that never open the network run as before. Task 2 step 7.

---

### Task 1: Thread primitives and the provider's up/down (fork)

**Files:**
- Modify: `~/workspaceToit/toit-v2.0.0/src/primitive.h` (`MODULE_ZEPHYR`, append three entries)
- Modify: `~/workspaceToit/toit-v2.0.0/src/resources/zephyr.cc` (the three primitives, moved from ztoit
  `zephyr/src/thread.cc` `attach()`)
- Modify: `~/workspaceToit/toit-v2.0.0/system/extensions/zephyr/network.toit`

**Interfaces:**
- Produces, in `MODULE_ZEPHYR`, all `FAIL(UNSUPPORTED)` without `CONFIG_NET_L2_OPENTHREAD`:
  - `thread_up, 0` → null. Non-blocking. First call: `openthread_init`, `openthread_run`, `net_if_up` (as
    `attach()` today). Later calls: `otThreadSetEnabled(true)` if disabled. Always sets the link mode
    rx-on-when-idle (undoing a SED "down").
  - `thread_role, 0` → int, `otThreadGetDeviceRole` (child = 2, router = 3, leader = 4).
  - `thread_down, 0` → bool, true if detached. With `CONFIG_TOIT_THREAD_DOWN_DETACH`: `otThreadSetEnabled(false)`.
    Otherwise: link mode rx-off-when-idle, and poll period = `otThreadGetChildTimeout() * 1000 / 2` ms.
  - Every OpenThread call under `openthread_mutex_lock`/`unlock`.
- Produces, in `network.toit`:
  - `ThreadModule implements NetworkModule`: `connect` calls `thread_up`, then polls `thread_role` every 100 ms
    until ≥ 2, or throws `"thread: attach timed out"` after 60 s. It prints `thread: up role=<r> in <ms> ms`.
    `disconnect` calls `thread_down` and prints `thread: down sed` or `thread: down detached`.
  - `ZephyrNetworkProvider` holds `state_ ::= NetworkState`. `connect client` does
    `state_.up: ThreadModule`, returns a stock `NetworkResource this client state_` (its `on-closed` calls
    `state_.down`), with the same `try`/`finally` `critical-do: state_.down` on exception as `WifiServiceProvider`.
    `ZephyrNetworkResource` is deleted. The provider registers with `--tags=["thread"]` (stock has no
    `TAG-THREAD`; WiFi uses `NetworkService.TAG-WIFI`), so an app can select Thread by tag.
- Produces, in ztoit `modules/toit/Kconfig`: `config TOIT_THREAD_DOWN_DETACH`, bool, default n,
  `depends on NET_L2_OPENTHREAD`, help: "Down = Thread disabled (each net.open rejoins); default: stay attached
  as a sleepy end device (design §7)." (Committed in Task 2.)

- [ ] **Step 1: Add the primitives** per Interfaces; `thread.cc`'s `attach()` is the reference for the init
  sequence.
- [ ] **Step 2: Rebuild the fork compiler.** Run: `tools/fork-sdk.sh`. Expected: exit 0.
- [ ] **Step 3: Write `ThreadModule` and the new provider** per Interfaces. Primitive stubs are
  `#primitive.zephyr.thread-up` etc.
- [ ] **Step 4: Compile the system program on the host.**
  Run: `$TOIT_HOST compile --snapshot -O2 -o /tmp/claude-1000/zsys.snapshot $TOIT_SRC/system/extensions/zephyr/boot.toit`
  Expected: exit 0.
- [ ] **Step 5: Commit** in the fork, `Zephyr: Thread up on the first net.open, down after the last close`, and
  push to the fork.

### Task 2: ztoit drops the boot-time attach; proven on the C6

**Files:**
- Delete: `zephyr/src/thread.cc`, `zephyr/src/thread.h`; modify `zephyr/src/main.cc` (the `ztoit_thread_up`
  call and include), `zephyr/CMakeLists.txt:5`.
- Modify: `modules/toit/Kconfig` (Task 1's `TOIT_THREAD_DOWN_DETACH`), `west.yml` (fork pin), `README.md`
  (Thread section: the node joins on the first `net.open`, not at boot).
- Create: `zephyr/toit/net_cycle.toit`: forever: time `net.open`, print `net_cycle: open <ms> ms`; one UDP echo
  to the soak's `GATEWAY`:`PORT` (3 s timeout), print `net_cycle: echo ok <ms> ms` or `net_cycle: echo failed`;
  close; sleep 60 s.
- Create: `zephyr/toit/net_crash.toit`: sleep 10 s, `net.open`, print `net_crash: open`, throw
  `"net_crash: on purpose"` without closing.

**Interfaces:**
- Consumes: Task 1's fork commit (pin), log lines `thread: up role=… in … ms`, `thread: down sed|detached`.

- [ ] **Step 1: Delete `thread.cc`/`thread.h` and the call**, add the Kconfig option, bump the pin.
- [ ] **Step 2: Write the two test containers.**
- [ ] **Step 3: Build for the C6.**
  Run: `source env.sh && west build -p -b esp32c6_devkitc/esp32c6/hpcore -d build zephyr -- -DEXTRA_CONF_FILE="prj_thread.conf;prj_thread.local.conf" -DCONTAINERS="$PWD/zephyr/toit/net_cycle.toit;$PWD/zephyr/toit/net_crash.toit"`
  Expected: success; record FLASH/RAM.
- [ ] **Step 4: Run it (ask David for the go).** Log with `tools/serial_log.py` to
  `~/.local/state/ztoit/net-c6-<date>.log` for 5 min. With `grep -a` expect:
  - no `thread:` line before the first `net_cycle: open`;
  - first `thread: up role=2 in <a few s> ms`, then `echo ok`, then `thread: down sed`;
  - later cycles: `net_cycle: open` in < 1 s (still attached as SED), `echo ok`, `down sed`.
  - Gateway: `ssh gw85224-01 docker exec otbr ot-ctl child table` lists the C6 (MAC 40:4c:ca:44:6e:60's
    extaddr) with R = 0 between cycles.
- [ ] **Step 5: Overlap and crash (Review Focus 2, 3).** In the same log:
  - `net_crash: open`, its trace, and after it `thread: down sed` still follows net_cycle's next close. A leaked
    resource from the dead container would hold the count at ≥ 1, and no `down` would print again.
  - Rebuild adding `soak.toit` as a third container (go needed): `udp ok` keeps counting and there is **no**
    `thread: down` at all for 5 min, though net_cycle closes every minute.
- [ ] **Step 6: Detach and attach failure (Review Focus 1).**
  - Rebuild with `-DCONFIG_TOIT_THREAD_DOWN_DETACH=y` and net_cycle only (go needed). Expect `thread: down
    detached`, each `net_cycle: open` taking seconds, and `echo ok` each cycle. Record the open times.
  - Rebuild without `prj_thread.local.conf` (no dataset), net_cycle only. Expect
    `thread: attach timed out` 60 s after the open and net_cycle's trace (it dies on the throw). Add a second
    `net.open` in a `catch` loop only if David wants the retry proven on hardware; the count's return to 0 is
    stock `NetworkState.up`.
- [ ] **Step 7: No network users, no Thread (Review Focus 4, 5).** Restore the C6's Plan 1 set:
  `-DCONTAINERS="hello.toit;crash.toit;io.toit"` (full paths) **with** Thread conf: the log has no `thread:` line.
  Then build the same set **without** Thread conf: it builds and runs as in Plan 1. Leave the C6 on the
  with-Thread build.
- [ ] **Step 8: Commit** ztoit: `Thread joins on the first net.open, down after the last close (refs #10)`.
  Comment the open times (SED vs detached) and FLASH/RAM on #10.

### Task 3: nRF52840 regression

**Files:**
- Modify: `docs/design/2026-11-15-nodus-toit-design.md`: under §11 step 2, one line with the measured open
  times and the nRF FLASH/RAM.

- [ ] **Step 1: Build for the dongle**, soak as the one container.
  Run: `west build -p -b nrf52840dongle/nrf52840 -d build-nrf zephyr -- -DEXTRA_CONF_FILE="prj_thread.conf;prj_thread.local.conf" -DCONTAINERS=$PWD/zephyr/toit/soak.toit`
  Expected: success; FLASH/RAM within Plan 1's (464,608 B / 186,896 B) ± the primitives.
- [ ] **Step 2: Run it (ask David for the go and which dongle; #9/#12 may own them).** Restart the logger after
  the flash. Expect `thread: up role=2`, then the first summary with `udp ok=37`, and no `thread: down` (the
  soak never closes).
- [ ] **Step 3: Record and commit** the design-doc line: `nodus-toit: Thread up/down measured (refs #10)`.
