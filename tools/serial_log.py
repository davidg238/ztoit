#!/usr/bin/env python3
# Log the C6's console with host timestamps, forever (soaks). Opening the port
# does not reliably reset the board (2026-11-15: a log opened mid-run): to see a
# boot, start the logger, then press RESET, or flash and log in one command.
#   tools/serial_log.py <logfile> [port]
import serial, sys, time
path = sys.argv[1]
port = sys.argv[2] if len(sys.argv) > 2 else '/dev/ttyUSB0'
s = serial.Serial(port, 115200, timeout=1)
with open(path, 'a', buffering=1) as out:
    out.write(f"{time.strftime('%Y-%m-%dT%H:%M:%S')} serial_log: opened {port}\n")
    while True:
        line = s.readline()
        if line:
            text = line.decode(errors='replace').rstrip()
            out.write(f"{time.strftime('%Y-%m-%dT%H:%M:%S')} {text}\n")
