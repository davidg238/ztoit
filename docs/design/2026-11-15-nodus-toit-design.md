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
  3. agent pull/push against porta's CoAP listener (rete#505);
  4. install;
  5. console;
  6. power states;
  7. the §9 tests;
  8. faber.
