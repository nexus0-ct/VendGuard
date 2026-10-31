#!/usr/bin/env python3
"""Decode actual UART transitions; no firmware text or transitions are generated."""
import argparse
import bisect
import hashlib
import json
from pathlib import Path
import re

p=argparse.ArgumentParser()
p.add_argument('vcd',type=Path)
p.add_argument('--output',type=Path,required=True,help='prefix for decoded JSON/TX/RX text')
p.add_argument('--baud',type=float,default=115200)
args=p.parse_args()
if args.baud<=0:p.error('baud must be positive')
names={};channels={'UART_TX':[],'UART_RX':[]};stamp=0;ns_per_tick=None
units={'s':1e9,'ms':1e6,'us':1e3,'ns':1,'ps':.001}
for line in args.vcd.open():
    line=line.strip()
    if line.startswith('$timescale'):
        m=re.search(r'(\d+)\s*(ms|us|ns|ps|s)',line)
        if m:ns_per_tick=int(m[1])*units[m[2]]
    elif line.startswith('$var'):
        f=line.split();names[f[3]]=f[4]
    elif line.startswith('#'):stamp=int(line[1:])
    elif line[:1] in ('0','1') and line[1:] in names:
        name=names[line[1:]]
        if name in channels:
            dest=channels[name];value=int(line[0])
            if not dest or dest[-1][1]!=value:dest.append((stamp,value))
if ns_per_tick is None:raise SystemExit('missing supported timescale')
def decode(transitions):
    if not transitions:return {'bytes':0,'lines':[],'framing_errors':0}
    times=[t*ns_per_tick for t,v in transitions];levels=[v for t,v in transitions]
    def value_at(t):return levels[max(0,bisect.bisect_right(times,t)-1)]
    bit=1e9/args.baud;available=0;previous=levels[0];raw=[];errors=0
    for t,level in zip(times,levels):
        if previous==1 and level==0 and t>=available:
            if value_at(t+bit*.5)==0 and value_at(t+bit*9.5)==1:
                byte=sum(value_at(t+bit*(1.5+i))<<i for i in range(8))
                raw.append((t,byte));available=t+bit*9.6
            else:errors+=1
        previous=level
    lines=[];text='';start=None;end=None
    for ns,byte in raw:
        if start is None:start=ns
        end=ns+bit*10
        if byte==10:
            lines.append({'start_ns':start,'end_ns':end,'text':text});text='';start=None
        elif byte!=13:text+=chr(byte)
    if text:lines.append({'start_ns':start,'end_ns':end,'text':text,'incomplete_line':True})
    return {'bytes':len(raw),'lines':lines,'framing_errors':errors}
result={'source':str(args.vcd.resolve()),'source_sha256':hashlib.sha256(args.vcd.read_bytes()).hexdigest(),
        'decoder_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'baud_assumed':args.baud,'timescale_ns':ns_per_tick,
        'channels':{k:decode(v) for k,v in channels.items()},
        'note':'Digital VCD decoding with center-of-bit sampling, 8N1. Host arrival times are not used. Derived text is not an original serial capture.'}
args.output.parent.mkdir(parents=True,exist_ok=True)
args.output.with_suffix('.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
for channel in channels:
    args.output.with_suffix('.'+channel.lower()+'.txt').write_text('\n'.join(x['text'] for x in result['channels'][channel]['lines'])+'\n')
print(json.dumps({'source_sha256':result['source_sha256'],'TX_bytes':result['channels']['UART_TX']['bytes'],'RX_bytes':result['channels']['UART_RX']['bytes'],'TX_framing_errors':result['channels']['UART_TX']['framing_errors']},ensure_ascii=False))
