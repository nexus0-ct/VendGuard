#!/usr/bin/env python3
"""Preserve a real recording under the owner's Lab03/Work, refusing overwrite."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess

p=argparse.ArgumentParser()
p.add_argument('name',help='unique run name, e.g. S1-20261028-01')
p.add_argument('--source',type=Path)
p.add_argument('--notes',required=True,help='actual actions and known capture limits')
args=p.parse_args()
if not re.fullmatch(r'[A-Za-z0-9_-]+',args.name):p.error('name must be a simple unique run identifier')
root=Path(__file__).resolve().parents[1];subject=root.parents[2]
source=(args.source or root/'docs/captures/lab3/vendguard-current.vcd').resolve()
if not source.is_file() or not source.is_relative_to(subject):p.error('recording must exist inside this owner SP directory')
out=subject/'Lab03/Work/captures';out.mkdir(exist_ok=True)
target=out/(args.name+'.vcd');manifest=target.with_suffix('.manifest.json')
if target.exists() or manifest.exists():p.error('refusing to overwrite an existing run')
data=source.read_bytes();sha=hashlib.sha256(data).hexdigest()
old=subject/'Lab03/Work/reference/pre-lab3-root-wokwi.json'
if old.exists() and json.loads(old.read_text())['sha256']==sha:p.error('this is the preserved Lab2 cache, not a new Lab3 recording')
if not data.startswith(b'$version') or b'$enddefinitions' not in data:p.error('not a supported VCD recording')
header=data.split(b'$enddefinitions',1)[0].decode(errors='replace')
build=subprocess.check_output(['python3',str(root/'tools/source_id.py')],text=True).strip()
elf=root/'firmware/build/firmware.elf'
if build.encode() not in elf.read_bytes():p.error('current ELF does not match firmware input fingerprint')
shutil.copy2(source,target)
assert hashlib.sha256(target.read_bytes()).hexdigest()==sha
record={'archived_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'name':args.name,
        'archiver_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'source_path':str(source),'recording_path':str(target),'recording_sha256':sha,'bytes':len(data),
        'vcd_header':header,'build_expected':build,'elf_sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),
        'diagram_sha256':hashlib.sha256((root/'diagram.json').read_bytes()).hexdigest(),
        'base_head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
        'notes':args.notes,'verification':'Expected build is recorded here; confirm actual BOOT/VERSION from decoded UART or original serial log. No transition has been changed.'}
manifest.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
print(manifest)
