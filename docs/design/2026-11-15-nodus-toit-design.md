# nodus-toit — Toit containers on Zephyr, managed through porta

**Status:** design, agreed with David 2026-11-15 in conversation; tracked as ztoit#10, roadmap step 2 of #11.
Porta's side is davidg238/rete#505. Precedent: the retired Toit keeper (`davidg238/nodus`,
`src/supervisor.toit`) and porta v1 (rete tag `porta-v1-final`).

## 1. Intent

A jag that works with sleepy nodes. The retired nodus proved this is very useful: a developer installs and runs
Toit containers on deployed, battery-powered nodes, and sees their console and crashes, without a cable and
without the node staying awake for a tool. ztoit nodes do this through rete's porta gateway (always on, holds a
mirror of each node's state, serves signed images). The developer's tool is **faber** (own repo, later design).

Toit has no debugger or profiler, so a **remote console** on deployed code is a requirement, not a nicety.

## 2. Goals, non-goals, anti-goals

- **Goals.** Install, start, stop, uninstall Toit containers over the air. Remote console and crash reports,
  surviving reboots. Nodes that are `on`, `sleep` or `deep-sleep`, reporting on their own schedule. Thread now,
  WiFi later. A proven crash-recovery path (§9). Stock Toit container compatibility (images,
  `system.containers`); jag/Artemis *wire* compatibility is not a goal (David 2026-11-15).
- **Non-goals.** Firmware OTA (the Zephyr image, the VM, the system program): deferred, as in rete's custos D4.
  App telemetry: apps that want it use their own CoAP client against porta's general CoAP facility; we are not
  responsible for it. Jaguar/Artemis protocol compatibility.
- **Anti-goals.** Changing the Toit world: apps are ordinary Toit programs using stock APIs, with no
  ztoit-specific calls needed. Making a fuss of our infrastructure: anything a client sees is plain CoAP, never
  porta's path vocabulary. Keeping the radio up for the tool's sake (jag's flaw).

## 3. Constraints

- Boards: nRF52840 (1 MB internal flash, 256 KB RAM), ESP32-C6, RP2350B (no radio). Program registry today
  is a devicetree `toit_partition`.
- ztoit today has **no containers**: `zephyr/toit/boot.toit` is a minimal system process that imports the app;
  `zephyr/src/main.cc` installs that one program into the registry and runs it. `main.cc` brings Thread up before
  the VM (`ztoit_thread_up`).
- The VM core already compiles the flash-registry and image-writer primitives (`primitive_flash_registry.cc`).
- New primitives go in the fork's `zephyr` module, appended last (`docs/design/zephyr-port.md`, #7 addendum).
- Porta is language-neutral and stays so; its manifest gates every path (`rete/protocol/MANIFEST.cddl`).

## 4. Approaches considered (decided)

| Question | Options | Picked |
|---|---|---|
| What is installed | A: the whole program image (one app, reboot per update); **B: containers** | **B**, as the retired work: containers only. Not waiting for upstream: Toit's own `system/containers.toit` + `system/flash/*` are existing Toit code, and the primitives are already built |
| Where lifecycle logic lives | 1: all in C; **2: split**; 3: all in Toit (the retired keeper) | **2**: C agent does transport, images, console, timing; the Toit system process runs the supervisor (ported from the retired `supervisor.toit`). A C safe mode covers a broken Toit side (§9) |
| Transport to porta | TFTP (retired); porta's current UDP framing; **CoAP** | **CoAP**: pull = `GET /space?id=&cursor=`, push = `POST /space`, images by blockwise GET (RFC 7959). The ratified direction in rete `PROTOCOL-NSL.md` §5.3 |
| Where the dev tool lives | inside custos; **own repo** | **faber**, own repo, depends on rete only for `protocol/` (custos design D2) |

## 5. Parts

| Part | Where | Owns |
|---|---|---|
| **C agent** | ztoit `zephyr/src/agent/` | CoAP client to porta: pull/push and blockwise image fetch. Writes images into the flash registry and verifies the ed25519 signature before commit. Flash-backed console. Boot counting and safe mode. Exposes primitives to the supervisor |
| **System program** | ztoit `zephyr/toit/` | Toit's stock `system/` (container manager, services, flash registry), plus our supervisor and network-service providers. Replaces today's minimal `boot.toit` |
| **Supervisor** | Toit, in the system program | Applies `goal/apps/*` and `goal/mode`: install/uninstall via the registry, start/stop, `run-once`/`run-loop`, triggers, runlevels, watchdog; reports `obs/apps/*`, `obs/mode`. Decides when to report and when to sleep |
| **Network providers** | Toit, in the system program | Thread now (WiFi later): the stock Toit network-service model, radio up on the first `net.open`, down after the last close |
| **porta** | rete, rete#505 | CoAP listener (IPv6 and IPv4), Toit image kind, signing (`porta blob put`) |
| **faber** | own repo, later | `run`/`install`/`uninstall`, `mode`, `monitor -f`, `panic` with `toit decode` against the snapshot |

## 6. Paths

Porta's manifest already carries the retired vocabulary; reuse it unchanged where it fits:

- `goal/apps/<name>` ↔ `obs/apps/<name>`: `sig`, `image` (blobref), `runlevel`, `triggers`, `arguments`,
  `lifecycle`.
- `goal/mode` ↔ `obs/mode`: today `mode`, `maxAwakeS`, `minAwakeS`, `loopSleepS`, `maxAsleepS`. `mode`'s
  values widen to `on` | `sleep` | `deep-sleep` (no second field), and it **adds** `reportS` (§7).
  *(Claude's call, after review.)*
- `goal/name`, `obs/health`, `sys/panic`, `tel/log` (the console), as for nsl nodes.
- **Adds `sys/trace`**, a stream of the program id and the Toit trace in pieces of at most 256 B each.
  `sys/panic`'s `detail` (about 60 B) cannot carry one. Part of rete#505. *(Claude's call, after review.)*
- `obs/build` reports the VM/SDK version. faber refuses an image built for a different one unless `--force`, as
  the retired nodus did, and the agent refuses a mismatch too. *(Claude's call, after review.)*

Paths are private to porta ↔ agent. Toit apps never see them. A new agent function is a new path in
`MANIFEST.cddl` plus a handler, never a new verb.

## 7. Power and reporting: two separate settings

**Power state**, chosen per board; each board declares which it supports, and a `goal/mode` asking for an
unsupported one is refused and reported:

| `power` | nRF52840 | ESP32 |
|---|---|---|
| `on` | always awake | always awake |
| `sleep` | CPU idle (tickless), RAM and VM kept, µs wake. Usually good enough | not offered |
| `deep-sleep` | System OFF, full boot on wake. Rarely worth it | essential, full boot on wake |

A **wake** is an interrupt (a trigger) or `maxAsleepS` elapsing. On a wake the supervisor runs the `run-once`
apps and their triggers, bounded by `minAwakeS`/`maxAwakeS`, then sleeps again.

**The radio belongs to the apps**, as in stock Toit. It comes up on the first `net.open` and goes down after the
last close. An app that never opens the network never turns the radio on.

**"Down" on Thread is a per-node setting** (David 2026-11-15):

- **Default: stay attached as a sleepy end device** that polls its parent rarely, every few minutes. `net.open`
  is then instant, and porta's writes wait at the parent.
- **The alternative: fully detach** (Thread disabled; each `net.open` rejoins, which takes a few seconds and can
  fail). It suits nodes that report a few times a day.

Today `main.cc` brings Thread up before the VM; under this design the Thread provider does that instead.

**Addendum 2026-11-15 (Plan 2 final review, #10):** the node is a sleepy end device always; "up" and "down"
change only the poll period (100 ms up, half the child timeout down), because flipping rx-on/off stores to flash
each time and the first switch forces a re-attach (OpenThread `mle.cpp` SetDeviceMode). A failed attach disables
Thread. Open: a first attach fails once the VM is up (#19), so the first join may have to stay in `main.cc`
before the VM (record: `2026-11-15-thread-first-attach.md`).

**Reporting** follows the radio:

- **Scheduled:** every `reportS` (e.g. 6 h), the supervisor opens the network itself and has the agent pull and
  push, then closes it.
- **Piggyback:** whenever any app has the network open, the agent checks in. An app that wants to report now
  simply opens the network, with no new API.

Between reports, console, crash reports and `obs/*` changes accumulate in flash. Porta's writes reach the node
at the next report. Offline is k × `reportS`.

## 8. Flows

**Install.**

1. faber builds and signs a container image (`porta blob put`) and writes `goal/apps/<name>`.
2. At the node's next check-in, the agent pulls the goal and fetches the blob blockwise into a fresh registry
   allocation, in the relocatable chunk format `main.cc` already writes.
3. The agent verifies SHA-256 and the ed25519 signature against porta's public key (built into the firmware),
   then commits the header last. An unverified or partial image is never valid.
4. The supervisor starts the container per its lifecycle and reports `obs/apps/<name>`.

**Console.**

- All VM stdout (apps and the system process) is written straight to a flash ring: Zephyr FCB or LittleFS,
  written directly, not via a RAM stage. It is sized per board: large where external flash exists, modest on
  the nRF52840.
- It is pushed as `tel/log` at each report. The ring survives reboots, so the lines before a crash reach porta.
- If the ring wraps before a report, the agent records the gap (lines dropped) rather than hiding it.

**Crash.**

- A Toit trace (today's `SYSTEM-TRACE_` handling) goes into `sys/panic` with the program id, so faber can decode
  it against the right snapshot.
- Zephyr faults are recorded at the next boot and pushed the same way.

## 9. Crash recovery (must be proven)

- **Boot counter** in flash or retained RAM: incremented at boot, cleared once the supervisor reaches healthy
  (system up, first check-in done).
- **N failed boots in a row → safe mode:**
  - first level: the system process boots with no containers started;
  - second level, if the system process itself keeps failing: the agent alone, in C, without the VM.
- **First-level safe mode** still reports and accepts goal changes, e.g. uninstall a bad container, so a fix can
  arrive over the air.
- **Second-level safe mode** only reports (console, panic, health). With firmware OTA deferred, a broken system
  process needs a cable, so the C side does not duplicate the supervisor. *(Claude's call, after review.)*
- **Tests to prove it**, each run on hardware with its outcome recorded:
  1. A container that crash-loops at start: other containers and reporting keep working; the crash is in
     `sys/panic`.
  2. A container that wedges the system process (e.g. allocates without bound): the node reaches safe mode and
     recovers after uninstall.
  3. Power cut during an image fetch and during the commit: the old state remains valid, and the fetch resumes
     by block number.
  4. A bad signature: refused, reported, and the old container keeps running.

## 10. Settled after review

- **Security.** No DTLS for now (David 2026-11-15). Images are ed25519-signed (porta signs the SHA-256,
  `porta/internal/blob/keys.go`), so nothing unsigned is installed. The exchange itself is unencrypted and the
  node is not authenticated. DTLS comes later, after its RAM cost on the nRF52840 is measured.
- **Porta's CoAP listener comes first.** The agent speaks CoAP from the start (David 2026-11-15), so rete#505's
  listener (IPv6 and IPv4) is built before the agent's pull/push, by us, in rete.
- **Boards.** The C6 first; then the nRF52840 once it is measured that the stock `system/` fits. The §9 tests
  run on both. The RP2350B is out of scope (no radio). *(Claude's call.)*

## 11. Left to the builder

- **Agent ↔ supervisor primitive set** (in the `zephyr` module), e.g. check-in now, read pulled goals, post
  `obs` cells, image status.
- **Console format:**
  - capture point, either the stdout primitive or the Zephyr console backend, provided system-process output is
    included;
  - ring size per board.
- **The safe-mode threshold N** and where the boot counter lives.
- **How the stock `system/` is trimmed** to fit the nRF52840 (measure first).
  - Measured 2026-11-15 (Plan 1, untrimmed stock system program + `soak.toit` as its one container): FLASH
    464,608 B of 636 KB (71%), RAM 186,896 B of 256 KB (71%). Registry images: system 49,152 B, soak 57,344 B,
    in a 240 KB registry. It fits untrimmed, and the soak's first 37 rounds pass (`udp ok=37`, `tcp ok=37`). No
    trimming needed yet.
- **Deep sleep on each SoC**: wake sources and what persists across it.
- **Order of work.** One sensible order:
  1. stock container system boots on ztoit;
  2. network provider up/down;
     - Measured 2026-11-15 (C6, `net_cycle.toit`): re-open as SED ~0.45 s, detached 2.0–2.7 s; attach timeout
       60 s. nRF52840 + soak: FLASH 464,448 B, RAM 187,152 B. Early echoes drop after attach (#18).
     - Measured 2026-11-16 (Plan 2 fixes A + I2, fork 08e79a85, always SED): C6 first attach from erased
       settings 1418 ms, from a stored child 212 ms; DK-base nRF52840 1107 ms, soak 666/666. The nRF52840
       dongle hangs at boot on the same VM (#23). C6 needs carrier-sense CCA: energy detect at -60 dBm drops
       frames after false busy (#18).
  3. agent pull/push against porta's CoAP listener (rete#505);
  4. install (it must replace Plan 1's boot rule in `main.cc`, which erases the registry unless it holds exactly
     the firmware's images, so a container installed at runtime would be erased on the next boot);
  5. console;
  6. power states;
  7. the §9 tests;
  8. faber.

## Addendum 2026-11-17: install, first slice (#31)

**Rule.** The agent fetches an image into a staging partition while the VM runs, verifies it there, and
reboots; `main()` installs the staged image into the registry before the VM starts, the way it installs a
firmware image. The registry is written only when no VM is running.

**Why not §8's "fetch into a fresh registry allocation".** The registry's allocator is the Toit system
process (`system/flash/registry.toit` keeps the free list it scanned at start). A C thread that writes a hole
while the VM runs can be handed the same hole by a Toit allocation (a `storage.Region`, a container install).
Doing it soundly needs either the supervisor to allocate (step later, not built) or the link brought up in C
before the VM. Staging needs neither. Cost: each image is written twice, one reboot per install, and an image
is limited to the staging partition. When the supervisor exists it can install from staging without the
reboot; staging stays as the place an image is verified before the registry sees it.

**Flow.**

1. *Check-in.* A `goal/apps/<name>` row whose `image.hash` differs from the installed record becomes the
   pending install (one at a time; a newer goal replaces it and restarts the fetch).
2. *Fetch.* `GET /blob/<hash>` by Block2 into the staging partition, continued across check-ins. Progress is
   the count of staged bytes, recovered after a reboot by scanning for the first erased block, so a power cut
   resumes by block number (§9 test 3). The staging partition is erased when a fetch starts.
3. *Verify.* SHA-256 over the staged bytes must equal `image.hash`, and `sig` must be an ed25519 signature of
   those 32 bytes under porta's public key. A failure erases staging, is printed, and is not retried for the
   same hash (§9 test 4). The key is `CONFIG_TOIT_AGENT_PUBKEY` (hex, deployment-specific, local conf); with
   no key nothing installs.
4. *Commit to staging.* The record `{name, hash, size, keep, state = verified}` is saved in settings, then
   the agent reboots after the check-in ends.
5. *Boot.* `main()`: firmware images first (rule below); then a `verified` record is installed with
   `install_image` from the staging partition into the first free run after the firmware images, replacing an
   allocation of the same name. The record becomes `installed {offset}` only after the header commit; a power
   cut before that repeats the install from staging.
6. *Run.* The stock container manager starts it at that boot (`FLAG-RUN-BOOT`). `keep = false`: the record is
   marked `ran` at that boot and the allocation is erased at the following one.
7. *Report.* The agent pushes `obs/apps/<name>` with the installed `image` at its next check-in.

**Boot rule (replaces Plan 1's).** The firmware's images sit back to back at the front of the registry. If
they are exactly there, everything after them is kept. If not, the whole registry and the agent's installed
records are erased and the firmware images reinstalled: installed apps come back from porta's goals, which
are the record of what should be on the node.

**Assumptions and limits, written down.**

- Staging is the board's existing `scratch_partition` (ESP32 default table: 124 KB at 0x3E0000, unused
  without MCUboot swap). No new partition until an image does not fit. Boards without one get no install yet.
- Something must open the network for the agent to check in (§7 piggyback): until the supervisor reports on
  its own, an install-capable firmware carries a boot container that opens the network periodically.
- An install kills whatever is running (the reboot). The supervisor will choose the moment later.
- Uninstall on a removed goal uses the same erase-at-boot path; it is in the slice only if the row shape for
  a removed goal is settled on the porta side.
- ed25519: mbedTLS in the tree has no EdDSA. Verify-only code is vendored (public domain), not written here.
- WROOM-32 over WiFi first. Thread boards follow when the agent's check-in runs over IPv6.

## Addendum 2026-11-17 (later): install as built (#31), SDK identity (#24)

Where the build differs from the addendum above, and why. Evidence is on #31.

- **Two records, not one.** `agent/stage` (fetching, verified, refused) and `agent/app/<name>` (installed, ran,
  erased), in `zephyr/src/agent/store.c`: with one record a refused or half-fetched image would overwrite what is
  installed under that name. `erased` is kept so a run-once goal does not install again.
- **One GET per block.** Zephyr's `coap_client` cannot start a Block2 transfer at block n > 0, so the agent
  asks for each block itself; resume is the first erased block minus one (a cut can leave a block half-written).
- **A fetch continues after `ztoit_link_going_down`** for the agent's `TIMEOUT_MS`: an app's `net.close` can
  wait that long while an image is fetching. Without it a 30 s cycle fetched about one block per cycle.
- **`main()` starts the agent** after the boot install, so the agent never touches the records while `main`
  changes them. A staged image that cannot be installed is marked refused, not a FATAL that loops every boot.
- **The agent runs on Thread too.** Sockets take the build's family; the check-in is owed on each `thread_up`
  (weak `ztoit_link_up()` in the fork); the agent's conf is `zephyr/prj_agent.conf`. The first check-in after
  boot fails before the attach (#32).
- **Run-once is the boot rule's, not a supervisor's.** `keep = false` is erased at the boot after it ran, and
  nothing is pushed after the erase, so `obs/apps/<name>` still shows the image. Open on #31.
- **SDK identity (§6, #24).** Node and host tool report `tools/sdk-id.sh`'s string, `<stock version>+zephyr.<fork
  commit>`; `tools/fork-sdk.sh` stamps it into `$TOIT_HOST version`, the firmware stamps it at build time
  (`zephyr/ztoit_ids.cmake`). A fork commit without a rerun of `fork-sdk.sh` is a mismatch by design.
