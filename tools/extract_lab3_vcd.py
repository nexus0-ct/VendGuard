#!/usr/bin/env python3
"""Export a labelled, time-preserving excerpt from a real Wokwi 1 ns VCD."""
import argparse
import hashlib
import json
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('source',type=Path)
p.add_argument('--start-us',type=float,required=True)
p.add_argument('--end-us',type=float,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--channels',nargs='+')
a=p.parse_args()
if a.start_us<0 or a.end_us<=a.start_us:p.error('require 0 <= start < end')
if a.output.exists() or a.output.with_suffix('.manifest.json').exists():p.error('refusing overwrite')
source_hash=hashlib.sha256(a.source.read_bytes()).hexdigest()
start=round(a.start_us*1000);end=round(a.end_us*1000)
header=[];names={};selected=set();levels={};events=[];stamp=0;in_data=False;seen_scale=False
with a.source.open() as stream:
    for raw in stream:
        line=raw.strip()
        if not in_data:
            if line.startswith('$timescale'):
                if line!='$timescale 1ns $end':p.error('this extractor requires a Wokwi 1ns capture')
                seen_scale=True
            if line.startswith('$var'):
                f=line.split();names[f[3]]=f[4]
                if a.channels is None or f[4] in a.channels:selected.add(f[3])
                else:continue
            header.append(raw)
            if line=='$enddefinitions $end':in_data=True
            continue
        if line.startswith('#'):
            stamp=int(line[1:])
            if stamp>end:break
        elif line[:1] in ('0','1','x','z') and line[1:] in selected:
            key=line[1:]
            if stamp<start:levels[key]=line[0]
            elif stamp<=end:events.append((stamp,line))
if not seen_scale or not selected:p.error('missing timescale or requested channels')
missing=set(a.channels or [])-set(names.values())
if missing:p.error('unknown channel(s): '+', '.join(sorted(missing)))
a.output.parent.mkdir(parents=True,exist_ok=True)
with a.output.open('w') as out:
    out.write(''.join(header[:-1]))
    out.write(f'$comment Derived excerpt of {a.source.name}; SHA256 {source_hash}; original absolute timestamps retained. Initial levels reconstructed from preceding observed transitions; unknowns remain x. $end\n')
    out.write(header[-1]);out.write(f'#{start}\n$dumpvars\n')
    for key in names:
        if key in selected:out.write(levels.get(key,'x')+key+'\n')
    out.write('$end\n');previous=start
    for stamp,line in events:
        if stamp!=previous:out.write(f'#{stamp}\n');previous=stamp
        out.write(line+'\n')
    if previous!=end:out.write(f'#{end}\n')
manifest={'source_name':a.source.name,'source_sha256':source_hash,
          'extractor_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
          'excerpt_sha256':hashlib.sha256(a.output.read_bytes()).hexdigest(),
          'start_ns':start,'end_ns':end,'original_events_copied':len(events),
          'channels':[names[k] for k in names if k in selected],
          'unknown_at_start':[names[k] for k in selected if k not in levels],
          'note':'Derived selection, not a new raw measurement. No transitions invented inside the window; absolute time is unchanged.'}
a.output.with_suffix('.manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(a.output)
