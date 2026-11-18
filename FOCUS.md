# Focus

- **Next session starts with (Opus):** branch `main`, focus **#37 ArduCAM**. Ask David what the wiring/power check
  found (camera VCC to 5 V, reseat the 6 wires). Then `jag flash` the camera WROOM-32 (router WiFi from
  `~/.claude/wifi.env`) and `jag run -d camera spike/arducam/jag_probe.toit`. If 0x40 answers (0x81 in 2025),
  flash `build-arducam` and run `spike/arducam/rx.py`. If it is still all `ff`, take #38 or #32 instead.
- **Boards:** camera WROOM-32 (by-path `…usb-0:1.3:1.0-port0`, MAC 84:cc:a8:11:b3:7c, needs its LiPo) is ztoit's
  for today's swap and runs `build-i35`; logger 2194523 (`i35-cam-20261118.log`). WiFi WROOM-32 (`…usb-0:1.4.1`,
  node 7c9ebdd8f58c) is faber-89's. C6 47a4 runs `build-i35-c6`; logger 2205311 (`i24-c6-20261117.log`).
  Dongle 2dbf logger 1730461 (#27). J-Link nRF52840: `rtt_drain.py` 1405701.
- **Zephyr tree:** `~/zephyr-esp32/zephyr` carries `patches/zephyr-nrf-802154-hfclk-ready-deferred.patch`
  uncommitted; re-apply after any west update (`docs/design/zephyr-port.md` addendum 2026-11-17).
- **Awaiting David:** #37 hardware check (tonight). Was dongle 2dbf touched around 20:00 on 2026-11-17 (#27)?
  The Thread network key in public rete: rotate or accept. #26 and #33 upstream reports. XIAO nRF54L15 Sense.
- **Decided:** run-once erase is pushed as `obs/apps/<name>` `lifecycle: #erased` (David 2026-11-18, #31).
  `davidg238/arducam_mega` retired: private and archived (David 2026-11-18, #37). Install stages, verifies,
  reboots, `main()` installs (design addenda 2026-11-17). Host tool stays private. No IPv6 on the WROOM-32.
  Uninstall waits for rete#519. Never halt a running nRF (#23).
- **Open P2:** #37, #23, #18, #8, #12, #4. **P3:** #2, #3, #5, #9–#11, #13–#17, #20–#22, #26–#29, #32, #33, #38.
