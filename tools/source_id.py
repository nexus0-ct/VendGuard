#!/usr/bin/env python3
"""Fingerprint the actual firmware inputs, including uncommitted changes."""
from pathlib import Path
import hashlib

root = Path(__file__).resolve().parents[1]
files = [p for p in (root / 'firmware').rglob('*') if p.is_file()
         and 'build' not in p.parts and 'Drivers' not in p.parts]
files += [root / 'diagram.json', root / 'wokwi.toml']
h = hashlib.sha256()
for p in sorted(files):
    h.update(str(p.relative_to(root)).encode() + b'\0' + p.read_bytes())
print('vg3-' + h.hexdigest()[:16])
