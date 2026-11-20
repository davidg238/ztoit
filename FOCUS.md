# Focus

- **Next session starts with (Opus):** branch `main`, #41 (ktau rung 5: LC3 encode primitive; clip size and encode
  time measured). Rung 4 is done (ktau `docs/journal/04-pdm-microphone.md`). First: write rung 5's plan with
  superpowers:writing-plans from the ktau design; find Zephyr 4.5's `liblc3` module and a sample that uses it,
  and run that as the first check (liblc3: `~/zephyr-4.5/modules/lib/liblc3`, no standalone sample; ktau is
  `~/workspaceToit/ktau`). At the bench: same short phrase ×5 into the Parrot headset and the XIAO at shirt
  distance, then faster-whisper medium.en on all (headset clip: −0.27 logprob; XIAO 1.5 s: −0.83, not comparable).
  Fallback: #37 after the resolder.
- **Boards:** XIAO nRF54L15 console `…XIAO_nrf54_CMSIS-DAP_E96922FA-if02`, runs the rung 4 overrun break-check
  (reflash `rung4.toit` from `~/zephyr-4.5/build-rung4` for the demo); logger 2649520, log
  `ktau-rung2-xiao-20261119.log`. Camera WROOM-32 (by-path `…usb-0:1.3:1.0-port0`) runs Jaguar at 192.168.0.244.
  C6 47a4 runs `build-i38-c6` (logger 2298951). Dongle 2dbf logger 1730461. J-Link `rtt_drain.py` 1405701.
- **Awaiting David:** #37 header resolder (then VCC 5 V if still `ff`); #26/#33 upstream reports on hold.
- **Decided:** No history scrub (2026-11-19). Thread key in public rete accepted (2026-11-18). Refused install
  pushed `#refused` (#38). Zephyr 4.5 beside 4.2; Mbed TLS 4 via header shim. I2C is one generic primitive; sensor
  interrupts are `gpio-keys`; the microphone is one generic `dmic` group, and `start` takes it over
  (`docs/design/zephyr-port.md` 2026-11-19). ktau: Toit owns the IMU; audio streams only after LC3; gain +10 dB,
  levels evened out at the receiver (ktau design, Decided). nRF: sleep, not deep sleep. Never halt a running nRF (#23).
- **Open P2:** #41, #37, #23, #18, #8, #12, #4. **P3:** #2, #3, #5, #9–#11, #13–#17, #20–#22, #26–#29, #33, #39,
  #40, #42–#45.
