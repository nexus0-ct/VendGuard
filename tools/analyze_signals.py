#!/usr/bin/env python3
"""Read actual VCD transitions, decode UART/I2C and measure digital markers."""
import argparse,bisect,hashlib,heapq,itertools,json,statistics
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('vcd');p.add_argument('--output',required=True);args=p.parse_args()
source=Path(args.vcd);text=source.read_text();header,body=text.split('$enddefinitions $end',1)
assert '$timescale 1ns $end' in header
symbols={line.split()[3]:line.split()[4] for line in header.splitlines() if line.startswith('$var ')}
channels={name:[] for name in symbols.values()};now=0
for line in body.splitlines():
 if line.startswith('#'):now=int(line[1:])
 elif len(line)>1 and line[0] in '01' and line[1:] in symbols:
  channel=channels[symbols[line[1:]]];value=int(line[0])
  if not channel or channel[-1][1]!=value:channel.append((now,value))
uart=channels['UART_TX'];times=[t for t,_ in uart];levels=[v for _,v in uart]
def at(t):return levels[max(0,bisect.bisect_right(times,t)-1)]
bit=1e9/115200;next_time=0;bytes_out=[];previous=0
for t,level in uart:
 if previous==1 and level==0 and t>=next_time and at(t+bit*.5)==0 and at(t+bit*9.5)==1:
  value=sum(at(t+bit*(1.5+i))<<i for i in range(8));bytes_out.append({'ns':t,'byte':value});next_time=t+bit*9.6
 previous=level
lines=[];line='';start=None
for b in bytes_out:
 if start is None:start=b['ns']
 if b['byte']==10:lines.append({'ns':start,'text':line});line='';start=None
 elif b['byte']!=13:line+=chr(b['byte'])
transactions=[];clock=1;data=1;current=None;bits=[]
merged=heapq.merge(((t,'SCL',v) for t,v in channels['SCL']),((t,'SDA',v) for t,v in channels['SDA']))
for t,group in itertools.groupby(merged,key=lambda x:x[0]):
 old_clock,old_data=clock,data
 for _,name,v in group:
  if name=='SCL':clock=v
  else:data=v
 if old_clock==clock==1 and old_data!=data:
  if data==0:
   if current is not None:current['end_ns']=t;current['ending']='RESTART';transactions.append(current)
   current={'start_ns':t,'bytes':[],'acks':[]};bits=[]
  elif current is not None:
   current['end_ns']=t;current['ending']='STOP';transactions.append(current);current=None;bits=[]
 elif old_clock==0 and clock==1 and current is not None:
  bits.append(data)
  if len(bits)==9:
   current['bytes'].append(sum(v<<(7-i) for i,v in enumerate(bits[:8])));current['acks'].append(bits[8]==0);bits=[]
for tr in transactions:
 if tr['bytes']:
  tr['address']=tr['bytes'][0]>>1;tr['read']=bool(tr['bytes'][0]&1)
  tr['data']=tr['bytes'][1:];tr['duration_us']=(tr['end_ns']-tr['start_ns'])/1000
rising={name:[t for t,v in vals if v==1] for name,vals in channels.items()}
periods=[b-a for a,b in zip(rising['SCL'],rising['SCL'][1:]) if 0<b-a<20000]
adc_periods=[b-a for a,b in zip(rising['ADC_SAMPLE'],rising['ADC_SAMPLE'][1:])]
widths=[];high=None
for t,v in channels['ADC_SAMPLE']:
 if v:high=t
 elif high is not None:widths.append(t-high);high=None
pir_pulses=[];high=None
for t,v in channels['PIR']:
 if v:high=t
 elif high is not None:pir_pulses.append({'start_ns':high,'width_ms':(t-high)/1e6});high=None
stats={'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'uart_bytes':len(bytes_out),
       'scl_hz':1e9/statistics.median(periods) if periods else None,
       'adc_period_ms':statistics.median(adc_periods)/1e6 if adc_periods else None,
       'adc_width_us':statistics.median(widths)/1000 if widths else None,'pir_pulses':pir_pulses}
result={'statistics':stats,'uart_lines':lines,'i2c':transactions}
prefix=Path(args.output);prefix.parent.mkdir(parents=True,exist_ok=True)
prefix.with_suffix('.json').write_text(json.dumps(result,indent=2));prefix.with_suffix('.uart.txt').write_text('\n'.join(x['text'] for x in lines)+'\n')
print(json.dumps(stats,indent=2))
for row in lines:
 if any(x in row['text'] for x in ['EVENT','CHECK','PROBE','BOOT','READY']):print(f"{row['ns']/1e9:.6f}s {row['text']}")
