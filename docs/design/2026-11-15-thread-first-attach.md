# A first Thread attach fails once the Toit VM is up (C6), 2026-11-15

Issue #19, found during #10 (Plan 2, `docs/superpowers/plans/2026-11-15-nodus-toit-plan2-network.md`), while fixing the final
review's I1/I2. **Resolved 2026-11-15: the mbedTLS heap was too small — see "Resolution" at the end; the best
guess and two dead ends below were wrong.** Logs are in
`~/.local/state/ztoit/net-c6-*-20261115.log` (read with `grep -a`).

## The failure

On the C6 (`esp32c6_devkitc/esp32c6/hpcore`, Zephyr `tuvm-c6-radio-fix` bc47c6f, OpenThread MTD), with the
OpenThread settings erased (`esptool.py --port /dev/ttyUSB0 erase_region 0x3b0000 0x30000`, then a flash or
`esptool chip_id` to leave download mode), a **first** attach times out after 60 s if the Toit VM has booted.
OpenThread runs: it makes ~8 attach attempts and sends ~140 frames, but receives only 4–29 frames and never
leaves role 1 (detached). Every Plan 2 pass before this one restored a child already stored in NVS, which hid it.

## Every run (C6, settings erased first unless noted)

| # | What ran | Dataset at attach | Result |
|---|---|---|---|
| 1 | Plan 1 (`1cb28dc`): attach in `main()` before the VM, rx-on | Kconfig fields (not commissioned) | ✓ 1.1 s, echo OK |
| 2 | Plan 2 `bb866c16`: `thread_up` from the VM, rx-on | Kconfig | ✗ timed out |
| 3 | Plan 2 + fix A (always SED, poll 100 ms) from the VM, ×4 | Kconfig | ✗ timed out ×4; I2 "down detached" works |
| 4 | Same, with link counters | Kconfig | ✗ tx 137, rx 4, CCA errors 5, attach attempts 8 |
| 5 | Gateway MAC counters around run 4's twin | — | `RxErrSec` 81→81, `RxErrNoUnknownNeighbor` 36→36: no security rejects |
| 6 | Bisect: fix A's setup + attach in `main()`, wait, then VM | Kconfig | ✓ 2.6 s; then the VM's `thread_up` re-ran `openthread_run`, role 1→2 in 2.6 s, echo OK, `down sed` |
| 7 | Full active TLVs (`ot-ctl dataset active -x`) via `otDatasetSetActiveTlvs` before `openthread_run`, from the VM | Commissioned | ✗ timed out (change removed) |
| 8 | Per-thread runtime stats during the attach | Kconfig | ✗; CPU 97 % idle; Toit 330 ms, main 97 ms, openthread 293 ms in 30 s |
| 9 | Attach started in `main()` at 98 ms, not awaited; VM boots during it | Kconfig | ✗ tx 149, rx 29 |
| 10 | VM up, idle 11 s, then the first `net.open` | Kconfig | ✗ tx 147, rx 4 |

Also measured, settings **not** erased (a stored child): the VM-driven attach restores in ~0.45 s; with
`TOIT_THREAD_DOWN_DETACH` each re-open re-attaches in 2.0–2.7 s; the dongle (2dbf) attaches from the VM the same
way. No nRF52840 has done a first attach from scratch under Plan 2 (DFU does not erase its settings at 0xdc000).

## Dead ends (ruled out, with the run)

- **Dataset / credentials:** channel 14, PAN 0x0437, name, xpanid and network-key hash match the gateway; the
  full TLV dataset fails the same (7); the gateway sees no security errors (5). ns-zephyr's "Kconfig fields are not
  a commissioned dataset" (ns-zephyr `308b1c3`) is true here (`otDatasetIsCommissioned` = 0) but is not the cause:
  1 and 6 attach from the same Kconfig fields.
- **CPU starvation by the VM** (VM interpreter at prio 14, main 0, OpenThread 8): 97 % idle (8).
- **A VM startup transient:** 11 s of idle VM first still fails (10).
- **Who calls `openthread_run`:** `main()` calling it fails too when the VM boots during the attach (9).
- **The C6 radio stop/teardown bug** (rete `thread_bringup.c` caveat): the fix (`esp32_stop` →
  `esp_ieee802154_sleep`) is in the tree.
- **Sleepy vs rx-on:** both fail (2, 3).
- **Partition overlap:** C6 storage 0x3b0000+0x30000, Toit registry 0x400000+4 MB; nRF storage 0xdc000+0x4000,
  registry 0xa0000–0xdc000. No overlap. (The dongle partition that clobbered credentials in rete/ns-zephyr was
  NCS's `settings_storage` on the Open Bootloader pages at 0xfe000; ztoit builds upstream Zephyr, not NCS.)
- **Gateway capacity:** child table 5 of 64.

## Best guess

Something the VM sets up at boot, and keeps, stops the C6 from **receiving** during parent discovery (Parent
Response / Child ID Response), while transmit, unicast traffic to an existing parent, and the child-restore path
keep working. Ranked:

1. `FlashRegistry::set_up()` (fork `src/flash_registry_zephyr.cc`): `spi_flash_mmap`s the whole 4 MB registry. It
   is the one MMU/cache change the VM makes. OpenThread writes settings during an attach, and an ESP flash write
   disables the cache; a mapping that changes what stays reachable then could cost radio RX.
2. The VM's event-source threads (Zsock poll, ZephyrKeys, Timer) or `OS::set_up()`: something that masks
   interrupts or holds a lock the 802.15.4 receive path needs. Note #5: the ESP 802.15.4 driver nests its critical
   section from the ISR.
3. A buffer/pool the VM's sockets take (`NET_BUF_RX_COUNT=16`, `NET_PKT_RX_COUNT=8`, ZVFS fds) that the
   attach's fragmented Child ID Response needs. Weak: rx stays near 0, not "received and dropped".

## Next tests, cheapest first (one C6 flash each)

1. Attach in `main()` with only `FlashRegistry::set_up()` done first. Fails → (1). Attaches → add `OS::set_up()`,
   then `load_platform_event_sources()`.
2. OpenThread log level NOTE: MLE prints Parent Request sent / Parent Response received and any settings-store
   failure. This says whether the response never arrives or arrives and is dropped.
3. The same first attach on an nRF dongle from scratch (needs its settings erased, which stops a soak), to learn
   whether this is C6-only.

## Fallback that ships regardless

David's proposal (2026-11-15): attach in `main()` before the VM, capped as Plan 1 was (60 s), then the provider's
up/down as Plan 2 built it. Measured cost: ~0.45 s per boot with a stored child, 1.1–2.6 s from scratch. Its risk:
a node that must find a new parent while the VM runs (parent lost) may hit this failure; the detached-mode
re-attach working with the VM up is partial evidence it will not.

## Resolution (2026-11-15, runs 11–13)

**Cause:** OpenThread shares Zephyr's mbedTLS heap (`CONFIG_MBEDTLS_HEAP_SIZE`, default 512 B) with the VM. Setting
the network key (`openthread_run` → `otThreadSetNetworkKey` → `KeyManager::UpdateKeyMaterial`) starts an HMAC, whose
`mbedtls_md_setup` allocates from that heap. With the VM up the allocation fails; OpenThread reports it only through
`SuccessOrAssert`, which is a no-op without `CONFIG_ASSERT`, so the keys are derived from garbage and the gateway
drops every MLE message. A stored dataset hid it because its keys are derived at boot, before the VM.

**Fix:** `CONFIG_MBEDTLS_HEAP_SIZE` in `zephyr/prj_thread.conf` (the value is a guess; its peak is not measured, #20).

| # | What ran (zephyr-port `bb866c16`, `thread_up` from the VM) | Result |
|---|---|---|
| 11 | + `CONFIG_ASSERT=y`, OT log INFO. Settings not erased first (no child stored since run 10) | `OpenThread ASSERT @ crypto/hmac_sha256.cpp:47` on the Toit thread at 1.04 s, in `UpdateKeyMaterial` |
| 12 | Run 11 + the heap line, settings erased | ✓ role 2 in 927 ms, echo OK; then the #5 spinlock assert at 3.56 s (`CONFIG_ASSERT` only) |
| 13 | Normal build + the heap line, settings erased | ✓ role 2 in 1519 ms, echo OK, `down sed`; next cycle up in 7 ms, echo OK |

Logs: `net-c6-otlog-`, `net-c6-heap2048-`, `net-c6-heapfix-20261115.log`.

**What the record above got wrong:**

- *Best guess (the VM stops the C6 receiving):* refuted by `net-c6-detach-20261115.log` — four re-attaches from the VM
  in 2.0–2.7 s, each receiving a Parent Response. The discriminator was erased settings, not the radio.
- *Dead end "dataset / credentials":* the gateway's MAC `RxErrSec` (run 5) cannot see an MLE-layer key failure, and the
  key-hash check compared the stored key, not the derived ones. Run 7 failed for the same reason as the rest.
- The fallback (attach in `main()` before the VM) is not needed and was not built.

**Not established:** who holds the heap once the VM is up (code read points at `EntropyMixer`: `process.cc` →
`mbedtls_entropy_func` keeps a hash context); why run 9 failed with the key set before the VM (rx 29, a different
signature). Both are in #20.

**Lesson:** a silent OpenThread failure gets one run with `CONFIG_ASSERT=y` before any bisecting.
