# nodus-toit Plan 1: Toit's container system boots on ztoit

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace ztoit's single boot program with Toit's stock container system, so apps are containers in the
flash registry, started at boot, each one able to crash without taking the others down.

**Architecture:** The fork gains `system/extensions/zephyr/boot.toit`, a Zephyr twin of the ESP32 system program
built on the stock `ContainerManager`. It also gains a Zephyr guard in `bundled_images`. ztoit's build compiles
that system program plus a list of containers (`-DCONTAINERS=a.toit;b.toit`), and embeds all of them.
`main.cc` installs the system program at registry offset 0 and each container after it, flagged run-at-boot.
Today's `PROGRAM`/`boot.toit` path is deleted.

**Tech Stack:** Zephyr 4.2, the Toit fork `~/workspaceToit/toit-v2.0.0` branch `zephyr-port`, ztoit CMake,
fork compiler `$TOIT_HOST` (`tools/fork-sdk.sh`).

**Spec:** `docs/design/2026-11-15-nodus-toit-design.md`. This is step 1 of its §11 order. Later plans:
network provider up/down, porta CoAP listener (rete#505), agent pull/push, install, console, power, the §9
tests, faber.

## Global Constraints

- Apps stay ordinary Toit programs: no ztoit-specific imports are needed in a container (design §2).
- Fork changes are pushed with `git push git@github.com:davidg238/toit.git HEAD:zephyr-port`. Never push to
  `origin` (that is upstream toitlang/toit).
- ztoit `west.yml` pins the fork commit. Bump the pin in the same ztoit commit that needs it.
- **Reflashing a board needs David's explicit go, every time.** The C6 is `/dev/ttyUSB0` (ztoit's on loan,
  currently running `io.toit`). Both dongles are mid-soak for #9.
- Serial logs need `grep -a`.
- Commits: `refs #10`, body ≤ 5 lines.

## Review Focus

1. **Containers left over from an earlier build.** A firmware with different bundled containers must not run
   containers left over in the registry from a previous build. Rule: when the system program's id differs from
   the one at offset 0, `main.cc` erases the whole registry before installing (dev bootstrap, until the agent
   owns containers). Tested in Task 2, step 6.
2. **An old single-program registry.** Boards flashed before this plan hold an old boot program at offset 0. It
   must be replaced, never listed as a container. This is covered by the same erase rule: a different id.
3. **A crashing container.** Its trace prints, the other containers keep running, and the system does not exit.
   Tested in Task 2, step 5.
4. **The registry is too small** for system plus containers: `FATAL` names the sizes, rather than a silent
   overlap. Tested in Task 2, step 3, by build-time arithmetic check.
5. **All containers exit.** `ContainerManager.wait-until-done` returns and `main.cc` prints `ztoit: exit reason`.
   That is acceptable in Plan 1 (an `on` node that stays up is the supervisor's job, a later plan). Asserted in
   Task 2, step 5, so the behaviour is known, not accidental.

---

### Task 1: The fork's Zephyr system program

**Files:**
- Create: `~/workspaceToit/toit-v2.0.0/system/extensions/zephyr/boot.toit`
- Create: `~/workspaceToit/toit-v2.0.0/system/extensions/zephyr/network.toit` (moved from ztoit
  `zephyr/toit/boot.toit`: `ZephyrNetworkProvider`, `ZephyrNetworkResource`, unchanged)
- Modify: `~/workspaceToit/toit-v2.0.0/src/primitive_programs_registry.cc:94-116` (`bundled_images`)

**Interfaces:**
- Produces: a compilable entry point `system/extensions/zephyr/boot.toit` whose `main`:
  1. calls `FlashRegistry.scan`;
  2. creates a `SystemServiceManager`;
  3. installs `ZephyrNetworkProvider`;
  4. creates a `ContainerManager`;
  5. registers a `SystemImage` (same shape as `system/extensions/esp32/boot.toit`, `id ::= containers.current`);
  6. calls `exit (boot container-manager)`.

  No firmware or storage providers in Plan 1.
- Produces: `bundled_images` returns `process->program()->empty_array()` under `TOIT_ZEPHYR`. Zephyr has no
  `EmbeddedData` extension, so the `TOIT_FREERTOS` branch would dereference a null pointer.

- [ ] **Step 1: Guard `bundled_images`.** Change the `#ifdef TOIT_FREERTOS` at line 95 to
  `#if defined(TOIT_FREERTOS) && !defined(TOIT_ZEPHYR)`.
- [ ] **Step 2: Write `network.toit`** by moving the two classes verbatim from ztoit `zephyr/toit/boot.toit`
  (the trailing `ZephyrNetworkProvider` and `ZephyrNetworkResource`).
- [ ] **Step 3: Write `boot.toit`** per the Interfaces block. Imports mirror the ESP32 one: `...boot`,
  `...containers`, `...flash.registry`, `...services`, `.network`.
- [ ] **Step 4: Compile it on the host.**
  Run: `$TOIT_HOST compile --snapshot -O2 -o /tmp/claude-1000/zsys.snapshot $TOIT_SRC/system/extensions/zephyr/boot.toit`
  Expected: exit 0, no errors.
- [ ] **Step 5: Commit** in the fork:
  `Zephyr: system program on the stock ContainerManager; no bundled images on Zephyr`.
  Push it to the fork: `git push git@github.com:davidg238/toit.git HEAD:zephyr-port`.

### Task 2: ztoit builds and installs the system program plus containers

**Files:**
- Modify: `zephyr/CMakeLists.txt` (program build rules)
- Modify: `zephyr/src/main.cc` (`install_program` → general install of N images)
- Delete: `zephyr/toit/boot.toit`
- Create: `zephyr/toit/crash.toit` (test container: prints `crash: start`, sleeps 2 s, throws `"crash: on purpose"`)
- Modify: `README.md` build commands (`-DPROGRAM=` → `-DCONTAINERS=`), `west.yml` (fork pin)

**Interfaces:**
- Consumes: Task 1's system entry point at `$TOIT_SRC/system/extensions/zephyr/boot.toit`.
- Produces:
  - CMake cache variable `CONTAINERS` (a `;`-list of `.toit` paths, default `toit/hello.toit`).
  - Each source compiles to `${GEN}/<stem>.image` via the existing `compile --snapshot` → `snapshot-to-image -m32`
    pair.
  - The images are concatenated into one generated include, `images.inc`, holding an array
    `{ const unsigned char* data; unsigned size; uint8 flags; }`: entry 0 is the system program (flags 0), the
    rest are containers (flags `1`, i.e. `ContainerService.FLAG-RUN-BOOT`).
- Produces: in `main.cc`:
  - `static const Program* install_image(const unsigned char* image, unsigned size, word offset, uint8 flags)`,
    the current `install_program` body generalised. `metadata[0] = flags`.
  - It returns the allocation; the caller places the next image at
    `offset + Utils::round_up(relocated_size, FLASH_PAGE_SIZE)`.

- [ ] **Step 1: Write the test container** `zephyr/toit/crash.toit` as described under Files.
- [ ] **Step 2: Rewrite the CMake rules** per Interfaces.
  - The system program compiles from `$TOIT_SRC/system/extensions/zephyr/boot.toit` in place, so nothing is
    copied.
  - Delete `PROGRAM` and the `boot.toit`/`app.toit`/`services.toit`/`network_base.toit` copies.
- [ ] **Step 3: Generalise `main.cc`.**
  - **Erase rule:** if the allocation at offset 0 is missing, or its id differs from image 0's, call
    `FlashRegistry::erase_chunk(0, FlashRegistry::allocations_size())` once, then install every image.
  - **Otherwise:** install only the images whose id is not already at its offset.
  - **Too big:** if the summed allocation sizes exceed `FlashRegistry::allocations_size()`, call
    `FATAL("images %d B > registry %d B", ...)`.
  - **Then** run image 0 as today (`run_boot_program`).
- [ ] **Step 4: Build for the C6 with three containers.**
  Run: `source env.sh && west build -p -b esp32c6_devkitc/esp32c6/hpcore -d build zephyr -- -DCONTAINERS="$PWD/zephyr/toit/hello.toit;$PWD/zephyr/toit/crash.toit;$PWD/zephyr/toit/io.toit"`
  Expected: the build succeeds and prints the FLASH/RAM summary. Record both numbers.
- [ ] **Step 5: Run it on the C6 (ask David for the go first).** Flash, then log with `tools/serial_log.py` to
  `~/.local/state/ztoit/containers-c6-<date>.log` for 60 s. With `grep -a`, expect all of these:
  - one `ztoit: program ... installed` line per image (4 lines) on the first boot;
  - the `hello` output;
  - `crash: start`, followed by a trace line (`ContainerManager` prints traces);
  - `io: ready`, and `bmp180:` lines **continuing after** the crash trace;
  - a BOOT press still toggles `do0` (David checks by eye);
  - no `ztoit: exit reason` while `io` runs.
- [ ] **Step 6: Prove the erase rule (Review Focus 1).**
  - Rebuild with `-DCONTAINERS=$PWD/zephyr/toit/hello.toit` only, flash (go needed), and log.
  - Expect: `hello` output, **no** `crash: start`, **no** `io: ready`, and then `ztoit: exit reason` once
    `hello` ends (Review Focus 5).
  - Rebuild with the three containers from step 4 and restore it, so the C6 is back to running `io.toit`.
- [ ] **Step 7: Commit** ztoit: `Toit containers on ztoit: stock ContainerManager, -DCONTAINERS, registry erase on a new system (refs #10)`.
  The commit includes the `west.yml` pin bump to Task 1's fork commit.

### Task 3: nRF52840 fit and soak as a container

**Files:**
- Modify: `docs/design/2026-11-15-nodus-toit-design.md`: append the measured numbers under §11 ("How the
  stock `system/` is trimmed").

- [ ] **Step 1: Build for the nRF52840 with the soak as its only container.**
  Run: `west build -p -b nrf52840dongle/nrf52840 -d build-nrf zephyr -- -DEXTRA_CONF_FILE="prj_thread.conf;prj_thread.local.conf" -DCONTAINERS=$PWD/zephyr/toit/soak.toit`
  Expected: the build succeeds. Record FLASH/RAM, and the image sizes from the `installed` lines once it runs.
  If it does not fit, stop and report the overflow to David. Trimming is a decision, not a guess.
- [ ] **Step 2: Run it on a dongle (ask David for the go and which dongle; #9's experiment may still own them).**
  Expect `soak: start` and the first `round 37` summary with `udp ok=37`. That proves the network works from a
  container.
- [ ] **Step 3: Record the numbers** in the design doc, comment them on #10, and commit:
  `nodus-toit: nRF52840 fits the container system (refs #10)`.
