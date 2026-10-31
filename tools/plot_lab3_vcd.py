#!/usr/bin/env python3
"""Plot actual digital transitions; no synthetic sensor samples."""
import argparse
import hashlib
import json
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

p=argparse.ArgumentParser()
p.add_argument('source',type=Path)
p.add_argument('--start-us',type=float,required=True)
p.add_argument('--end-us',type=float,required=True)
p.add_argument('--channels',nargs='+',required=True)
p.add_argument('--output',type=Path,required=True,help='PNG path; also writes SVG and provenance JSON')
a=p.parse_args()
if a.start_us<0 or a.end_us<=a.start_us:p.error('invalid time window')
outputs=[a.output,a.output.with_suffix('.svg'),a.output.with_suffix('.manifest.json')]
if any(f.exists() for f in outputs):p.error('refusing overwrite')
names={};prior={};events={name:[] for name in a.channels};stamp=0;scale_ok=False
for raw in a.source.open():
    line=raw.strip()
    if line.startswith('$timescale'):
        if line!='$timescale 1ns $end':p.error('requires Wokwi 1ns VCD')
        scale_ok=True
    elif line.startswith('$var'):
        f=line.split();names[f[3]]=f[4]
    elif line.startswith('#'):
        stamp=int(line[1:])
        if stamp>a.end_us*1000:break
    elif line[:1] in ('0','1') and names.get(line[1:]) in events:
        name=names[line[1:]];level=int(line[0]);us=stamp/1000
        if us<a.start_us:prior[name]=level
        else:events[name].append((us,level))
if not scale_ok or set(a.channels)-set(names.values()):p.error('missing timescale/channel')
labels={'MPU_TASK':'MPU task (PA6)','ADC_SAMPLE':'ADC (PA4)','PIR':'PIR (PA10)',
        'CUSTOMER_RESPONSE':'Response (PA8)','UART_TX':'UART TX (PA2)','UART_RX':'UART RX (PA3)',
        'AUDIT_TASK':'Audit (PA7)','HEARTBEAT':'Heartbeat (PA5)','LDR_DO':'Light step (LDR DO)'}
plt.rcParams.update({'font.size':11,'svg.fonttype':'none'})
fig,axes=plt.subplots(len(a.channels),1,figsize=(7.3,.65*len(a.channels)+.65),sharex=True,squeeze=False)
for ax,name in zip(axes[:,0],a.channels):
    points=[(a.start_us,prior.get(name,float('nan')))]+events[name]
    points.append((a.end_us,points[-1][1]))
    ax.step([(t-a.start_us)/1000 for t,v in points],[v for t,v in points],where='post',color='#165b8c',linewidth=1.2)
    ax.set_ylim(-.15,1.15);ax.set_yticks([0,1]);ax.set_ylabel(labels.get(name,name),rotation=0,ha='right',va='center',labelpad=14)
    ax.grid(axis='x',color='#d9d9d9',linewidth=.6)
    ax.spines[['top','right']].set_visible(False)
axes[-1,0].set_xlabel(f'Час у вікні, мс (початок VCD: {a.start_us/1e6:.6f} с)')
axes[-1,0].set_xlim(0,(a.end_us-a.start_us)/1000)
fig.subplots_adjust(left=.25,right=.985,top=.97,bottom=.22,hspace=.32)
a.output.parent.mkdir(parents=True,exist_ok=True)
fig.savefig(a.output,dpi=220,bbox_inches='tight',pad_inches=.08)
fig.savefig(a.output.with_suffix('.svg'),bbox_inches='tight',pad_inches=.08);plt.close(fig)
manifest={'source_name':a.source.name,'source_sha256':hashlib.sha256(a.source.read_bytes()).hexdigest(),
          'plotter_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
          'start_us':a.start_us,'end_us':a.end_us,'channels':a.channels,
          'known_levels_before_window':prior,'transitions_in_window':{n:len(v) for n,v in events.items()},
          'outputs_sha256':{f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in outputs[:2]},
          'note':'Derived scientific plot of real VCD. Unknown initial levels are NaN; time axis is relative to the labelled original window.'}
outputs[2].write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
print(a.output)
