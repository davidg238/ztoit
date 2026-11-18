#!/usr/bin/env python3
"""Pull the JPEGs that spike/arducam prints (base64 between JPEG-BEGIN n and
JPEG-END n) out of a serial log: rx.py LOG OUTDIR. Lines carry the logger's
timestamp prefix; the base64 is the last field."""
import base64, os, re, sys

log, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
frame, chunks = None, []
for raw in open(log, 'rb').read().decode('latin1').splitlines():
    text = raw.split(' ', 1)[1] if re.match(r'^\d{4}-\d\d-\d\dT', raw) else raw
    m = re.match(r'JPEG-BEGIN (\d+)', text)
    if m:
        frame, chunks = int(m.group(1)), []
        continue
    if frame is None:
        continue
    if text.startswith('JPEG-END') or text.startswith('JPEG-ABORT'):
        data = base64.b64decode(''.join(chunks))
        name = os.path.join(out, 'frame-%d.jpg' % frame)
        open(name, 'wb').write(data)
        ok = data[:2] == b'\xff\xd8' and b'\xff\xd9' in data[-64:]
        print('%s %d B, %s | %s' % (name, len(data), 'JPEG markers ok' if ok else 'NOT a whole JPEG', text))
        frame = None
    elif re.fullmatch(r'[A-Za-z0-9+/=]+', text.strip()):  # skips log lines that interleave
        chunks.append(text.strip())
    else:
        print('skipped:', text[:80])
