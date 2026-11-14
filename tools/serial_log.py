#!/usr/bin/env python3
# Log the C6's console with host timestamps, forever (soaks). Opening the port
# resets the board (DTR/RTS on this USB-UART), so a log starts at a fresh boot.
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
