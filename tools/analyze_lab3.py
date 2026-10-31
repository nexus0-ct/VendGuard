#!/usr/bin/env python3
"""Summarize saved MCU logs and independently measure Lab3 GPIO transitions."""
import argparse
import bisect
import hashlib
import json
from pathlib import Path
import re
import statistics

def summary(values):
    if not values:
        return {'n':0}
    s=sorted(values)
    return {'n':len(s),'min_us':s[0],'avg_us':statistics.mean(s),'max_us':s[-1],
            'p99_us':s[min(len(s)-1,(99*len(s)+99)//100-1)]}

def vcd_metrics(path, start_us=None, end_us=None, light_notifications=None):
    channels={}; levels={}; rises={}; falls={}; pulses={}; active={}; initial={}; adc_ends=[]; stamp=0; unit_ns=None
    scale={'s':1e9,'ms':1e6,'us':1e3,'ns':1,'ps':.001}
    for line in path.open():
        line=line.strip()
        if line.startswith('$timescale'):
            match=re.search(r'(\d+)\s*(ms|us|ns|ps|s)',line)
            if match: unit_ns=int(match[1])*scale[match[2]]
        if line.startswith('$var'):
            parts=line.split(); channels[parts[3]]=parts[4]
        elif line.startswith('#'):stamp=int(line[1:])
        elif line[:1] in ('0','1') and line[1:] in channels:
            key=line[1:]; name=channels[key]; value=int(line[0]); previous=levels.get(key)
            levels[key]=value
            if name in ('UART_TX','UART_RX'): continue
            if previous is None:initial[name]={'time_us':stamp*unit_ns/1000,'value':value}
            # Initial dumpvars is a level, not a measured edge.
            if previous is not None and value!=previous:
                if value:
                    us=stamp*unit_ns/1000
                    if (start_us is None or us>=start_us) and (end_us is None or us<=end_us):
                        rises.setdefault(name,[]).append(stamp)
                    active[name]=stamp
                elif name in active:
                    beginning=active.pop(name)
                    if (start_us is None or beginning*unit_ns/1000>=start_us) and (end_us is None or stamp*unit_ns/1000<=end_us):
                        pulses.setdefault(name,[]).append(stamp-beginning)
                        if name=='ADC_SAMPLE' and 'LDR_DO' in channels.values():adc_ends.append(stamp*unit_ns/1000)
                if not value:
                    us=stamp*unit_ns/1000
                    if (start_us is None or us>=start_us) and (end_us is None or us<=end_us):falls.setdefault(name,[]).append(us)
    if unit_ns is None:raise ValueError('unsupported or missing VCD timescale')
    metrics={'last_transition_us':stamp*unit_ns/1000,'channels':list(channels.values()),'method':'VCD transition timestamps',
             'window_start_us':start_us,'window_end_us':end_us,'window_note':'Only complete pulses and periods within the specified VCD window. None means entire capture, including startup and stats output.'}
    metrics['initial_observations']=initial
    metrics['initial_level_note']='A first observed high without a prior low is not counted as a proven rising edge. Some Wokwi captures omit initial levels.'
    for name in ('MPU_TASK','ADC_SAMPLE','AUDIT_TASK','CUSTOMER_RESPONSE'):
        ticks=rises.get(name,[])
        metrics[name]={'width':summary([x*unit_ns/1000 for x in pulses.get(name,[])]),
                       'period':summary([(b-a)*unit_ns/1000 for a,b in zip(ticks,ticks[1:])])}
    edges=rises.get('PIR',[]); responses=rises.get('CUSTOMER_RESPONSE',[]); delays=[]; trials=[]; used=-1
    for edge in edges:
        i=max(used+1,bisect.bisect_left(responses,edge))
        if i<len(responses):
            delays.append((responses[i]-edge)*unit_ns/1000);used=i
            trials.append({'edge_us':edge*unit_ns/1000,'response_us':responses[i]*unit_ns/1000,'latency_us':delays[-1]})
    metrics['pir_to_response']=summary(delays)
    metrics['pir_edges']=len(edges)
    metrics['pir_trials']=trials
    metrics['unmatched_pir']=len(edges)-len(delays)
    metrics['unmatched_response']=len(responses)-len(delays)
    metrics['latency_pairing_note']='FIFO pairing is valid only with equal event/response counts and zero dropped edges; otherwise latency association is uncertain.'
    if 'LDR_DO' in channels.values():
        opening=falls.get('LDR_DO',[]);closing=[t*unit_ns/1000 for t in rises.get('LDR_DO',[])]
        notes=light_notifications or [];trials=[];used_notes=set()
        for k,edge in enumerate(opening):
            close_index=bisect.bisect_right(closing,edge)
            close=closing[close_index] if close_index<len(closing) else (end_us if end_us is not None else stamp*unit_ns/1000)
            next_open=opening[k+1] if k+1<len(opening) else (end_us if end_us is not None else stamp*unit_ns/1000)
            found=next(((i,n) for i,n in enumerate(notes) if i not in used_notes and edge<=n['start_ns']/1000<next_open),None)
            row={'edge_us':edge,'close_us':close}
            if found is None:row.update({'notification_missing_before_next_open':True,'internal_event_unknown':True})
            else:
                i,note=found;used_notes.add(i);response=note['start_ns']/1000
                row.update({'notification_start_us':response,'notification_latency_us':response-edge,'event':note})
                index=bisect.bisect_right(adc_ends,response)-1
                if index>=0 and edge<=adc_ends[index]<=close:
                    row['sample_latency_lower_us']=adc_ends[index]-edge
                    row['verdict_500ms']=('met_by_notification_upper_bound' if response-edge<=500000 else
                                           'violated_by_sample_lower_bound' if row['sample_latency_lower_us']>500000 else
                                           'uncertain_between_sample_and_notification')
                else:row['sample_association']='uncertain'
            trials.append(row)
        metrics['light_trials']=trials
        metrics['light_notification_latency']=summary([t['notification_latency_us'] for t in trials if 'notification_latency_us' in t])
        metrics['light_pairing_note']='Controlled 1-lux/bright steps cross LDR_DO. Mode-change-triggered LDR events are excluded. Last ADC completion before a direct EVENT is a lower endpoint; first UART EVENT byte is an upper endpoint. Missing notification does not prove missing RAM event when TX errors occur.'
    return metrics

parser=argparse.ArgumentParser()
parser.add_argument('log',type=Path)
parser.add_argument('--vcd',type=Path)
parser.add_argument('--vcd-start-us',type=float)
parser.add_argument('--vcd-end-us',type=float)
parser.add_argument('--decoded-uart',type=Path,help='derive an interior measurement window from decoded TX MEASURE/METRICS markers')
parser.add_argument('--window-index',type=int,help='zero-based measurement window when a capture contains several')
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
text=args.log.read_text(errors='replace')
result={'log':str(args.log.resolve()),'log_sha256':hashlib.sha256(args.log.read_bytes()).hexdigest(),
        'analyzer_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'stats':[],'scheduler':[],'jitter':[],'memory':[],'counters':[],'measurement':[]}
for line in text.splitlines():
    target = ('stats' if line.startswith('STAT ') else 'scheduler' if line.startswith('SCHED ') else
              'jitter' if line.startswith('JITTER ') else 'memory' if line.startswith('MEM ') else
              'counters' if line.startswith('COUNTERS ') else 'measurement' if line.startswith('VG METRICS ') else None)
    if target:
        row={k:(int(v) if re.fullmatch(r'-?\d+',v) else v) for k,v in re.findall(r'(\w+)=([^\s]+)',line)}
        result[target].append(row)
if not result['measurement']:raise SystemExit('No actual VG METRICS block in input; refusing to invent results')
if args.vcd:
    result['vcd_sha256']=hashlib.sha256(args.vcd.read_bytes()).hexdigest()
    if args.decoded_uart:
        decoded=json.loads(args.decoded_uart.read_text())
        if decoded['source_sha256']!=result['vcd_sha256']:raise SystemExit('decoded UART belongs to another VCD')
        tx=decoded['channels']['UART_TX']['lines']
        starts=[r for r in tx if r['text']=='VG MEASURE start']
        ends=[r for r in tx if r['text'].startswith('VG METRICS ')]
        if not starts or not ends:raise SystemExit('need MEASURE and METRICS markers in the actual VCD')
        if len(starts)>1 and args.window_index is None:raise SystemExit('several measurement windows: select --window-index and supply its corresponding raw log')
        index=args.window_index or 0
        if index<0 or index>=len(starts):raise SystemExit('measurement window index out of range')
        beginning=starts[index]
        ending=next((r for r in ends if r['start_ns']>beginning['end_ns']),None)
        if ending is None:raise SystemExit('selected window has no METRICS marker')
        args.vcd_start_us=beginning['end_ns']/1000+1000
        args.vcd_end_us=ending['start_ns']/1000-1000
        if args.vcd_end_us<=args.vcd_start_us:raise SystemExit('invalid measurement marker order')
        result['vcd_window_source']={'decoded_uart':str(args.decoded_uart.resolve()),'window_index':index,'trim_us':1000,
                                    'note':'Interior window excludes ACK tail and stats dispatch/printing. Boundary quantization may omit a task.'}
    light_notes=[];previous_event=None
    if args.decoded_uart:
        for row in tx:
            match=re.fullmatch(r'VG EVENT seq=(\d+) ms=(\d+) category=(\w+) source=(\w+) event=(\w+) value=(-?\d+)',row['text'])
            if not match:continue
            seq,ms,category,source,kind,value=match.groups()
            event={'seq':int(seq),'ms':int(ms),'kind':kind}
            from_mode=(previous_event is not None and previous_event['kind']=='MODE_CHANGED' and previous_event['ms']==int(ms) and previous_event['seq']+1==int(seq))
            if source=='LDR_ADC' and kind in ('SERVICE_ACCESS','SERVICE_OPEN_SUSPECTED') and not from_mode:
                light_notes.append({**event,'category':category,'value':int(value),'start_ns':row['start_ns'],'end_ns':row['end_ns']})
            previous_event=event
    result['gpio']=vcd_metrics(args.vcd,args.vcd_start_us,args.vcd_end_us,light_notes)
    drops=[r['pir_dropped'] for r in result['counters'] if 'pir_dropped' in r]
    gpio=result['gpio']
    gpio['pir_pairing_valid']=(bool(drops) and all(n==0 for n in drops)
                               and gpio['pir_edges']==gpio['CUSTOMER_RESPONSE']['width']['n']
                               and gpio['unmatched_pir']==0 and gpio['unmatched_response']==0)
    gpio['pir_pairing_basis']={'cumulative_pir_dropped':drops,
                               'note':'Missing counters, unequal counts, or any dropped edges make inferred FIFO latencies provisional.'}
    if args.decoded_uart:
        operations=[]; records=[]
        for row in tx:
            if row['text'].startswith('VG RECORD '):records.append(row)
            elif row['text'].startswith('VG DONE op=journal ') and records:
                byte_count=sum(len(r['text'].encode('ascii'))+2 for r in records+[row])
                span_us=(row['end_ns']-records[0]['start_ns'])/1000
                operations.append({'start_us':records[0]['start_ns']/1000,'end_us':row['end_ns']/1000,
                                   'records':len(records),'output_bytes':byte_count,'span_us':span_us,
                                   'wire_min_us':byte_count*10*1e6/decoded['baud_assumed'],
                                   'extra_percent':(span_us/(byte_count*10*1e6/decoded['baud_assumed'])-1)*100})
                records=[]
        overlap=[]
        for trial in result['gpio']['pir_trials']:
            for index,op in enumerate(operations):
                if op['start_us']<=trial['edge_us']<=op['end_us']:
                    overlap.append({**trial,'journal_index':index});break
        result['journal_exports']=operations
        result['pir_during_journal']={'summary':summary([r['latency_us'] for r in overlap]),'trials':overlap,
                                      'pairing_valid':gpio['pir_pairing_valid'],
                                      'note':'Conservative overlap: PIR front between first RECORD byte and DONE stop bit. Pairing requires zero drops and equal event/response counts.'}
    result['comparison']=[]
    for stat in result['stats']:
        mapping={'mechanical':'MPU_TASK','light':'ADC_SAMPLE','pir_latency':'pir_to_response'}
        if stat.get('name') in mapping:
            key=mapping[stat['name']]
            gpio=result['gpio'][key] if key=='pir_to_response' else result['gpio'][key]['width']
            result['comparison'].append({'name':stat['name'],'firmware':stat,'gpio':gpio,
             'note':'Task span also includes dispatch/clock calls; GPIO excludes them. Light GPIO wraps ADC only, not detector.'})
args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(args.output)
