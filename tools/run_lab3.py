#!/usr/bin/env python3
"""Drive the real running VendGuard UART. Never generates MCU responses."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import random
import re
import subprocess
import time

import serial
from lab3_uart import status_times, metrics_complete

parser = argparse.ArgumentParser()
parser.add_argument('scenario', choices=['S1', 'S2', 'S3', 'fault', 'boundary'])
parser.add_argument('--seconds', type=float)
parser.add_argument('--port', type=int, default=4136)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--allow-short', action='store_true', help='development run, never an endurance result')
parser.add_argument('--s3-interval', type=float, default=2.0,
                    help='mean wall interval between S3 UART bursts; seeded jitter +/-25 percent')
parser.add_argument('--s3-status-count',type=int,default=20,help='status lines sent back-to-back during each real journal export')
parser.add_argument('--s3-handshake',action='store_true',help='resync with an empty line, then wait up to two seconds for actual journal DONE before the next cycle')
parser.add_argument('--s3-mode',choices=['guard','service'],default='guard',help='explicit mode after S3 journal prefill')
parser.add_argument('--boundary-burst',type=int,help='fixed number of status lines per one-second UART burst')
parser.add_argument('--fault-nack-seconds',type=float,default=6,help='time before restoring the correct MPU address in a fault run')
parser.add_argument('--wall-timeout', type=float, help='absolute capture timeout; default four times requested duration')
args = parser.parse_args()
duration = args.seconds if args.seconds is not None else {'S1':60,'S2':3600,'S3':120,'fault':12,'boundary':80}[args.scenario]
wall_timeout = args.wall_timeout if args.wall_timeout is not None else max(300,duration*4)
if duration <= 0 or (args.scenario == 'S2' and duration < 3600 and not args.allow_short):
    parser.error('S2 requires 3600 real seconds; use --allow-short only for development')
if args.s3_interval <= 0: parser.error('--s3-interval must be positive')
if args.s3_status_count<1:parser.error('--s3-status-count must be positive')
if args.boundary_burst is not None and args.boundary_burst<1:parser.error('--boundary-burst must be positive')
if args.fault_nack_seconds<=0:parser.error('--fault-nack-seconds must be positive')
root = Path(__file__).resolve().parents[1]
version = subprocess.check_output(['python3', str(root/'tools/source_id.py')], text=True).strip()
if args.output.exists() or args.output.with_suffix('.manifest.json').exists():
    parser.error('refusing to overwrite raw evidence')
args.output.parent.mkdir(parents=True, exist_ok=True)
commands = []
start = time.monotonic()
data = bytearray()
conn = serial.serial_for_url(f'rfc2217://localhost:{args.port}',baudrate=115200,timeout=0.05)
def pump(seconds):
    end = time.monotonic()+seconds
    while time.monotonic()<end:
        chunk = conn.read(4096)
        if chunk:
            data.extend(chunk)
            raw.write(chunk); raw.flush()
def send(command):
    commands.append({'host_elapsed_s':time.monotonic()-start,'ascii':command})
    conn.write(command.encode('ascii')+b'\n')
manifest = {'scenario':args.scenario,'requested_wall_seconds':duration,'build_expected':version,
            'runner_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            'uart_parser_sha256':hashlib.sha256((root/'tools/lab3_uart.py').read_bytes()).hexdigest(),
            'started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'source_head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
            'port':args.port,'seed':36,'commands':commands,'development_short':args.allow_short,
            's3_mean_burst_interval_s':args.s3_interval if args.scenario=='S3' else None,
            's3_status_count':args.s3_status_count if args.scenario=='S3' else None,
            's3_handshake':args.s3_handshake if args.scenario=='S3' else None,
            's3_mode':args.s3_mode if args.scenario=='S3' else None,
            's3_export_attempts':[],
            'boundary_fixed_burst':args.boundary_burst,
            'fault_nack_seconds':args.fault_nack_seconds if args.scenario=='fault' else None,
            'note':'Host time schedules stimuli only. Timing results must use MCU timestamps/VCD.'}
try:
    with args.output.open('wb') as raw:
        send('version'); pump(1)
        if f'VG VERSION build={version}'.encode() not in data:
            raise RuntimeError('running firmware fingerprint does not match working sources')
        for attempt in range(60):
            offset = len(data); send('status'); pump(1)
            if re.search(rb'calibrated=1 light_ready=1',data[offset:]): break
        else: raise RuntimeError('MPU and LDR did not finish calibration during warm-up')
        send('telemetry 0' if args.scenario=='S1' else 'telemetry 1'); pump(.2)
        if args.scenario=='S3':
            for i in range(64):
                send('service' if i%2==0 else 'guard'); pump(.06)
            send(args.s3_mode); pump(.15)
        send('measure on'); pump(.2)
        active = time.monotonic(); rng = random.Random(36); next_command = 0.0
        previous_status = status_times(data[-4096:])
        active_mcu_ms = int(previous_status[-1]) if previous_status else None
        def simulated_elapsed():
            latest = status_times(data[-4096:])
            return ((int(latest[-1])-active_mcu_ms)&0xffffffff)/1000 if latest and active_mcu_ms is not None else 0
        while time.monotonic()-active < duration or (args.scenario=='S2' and simulated_elapsed()<duration+2):
            t = time.monotonic()-active
            if t>wall_timeout: raise TimeoutError('capture timeout before requested duration; partial raw log retained')
            if args.scenario=='S2' and t>=next_command:
                send(['service','status','guard','probe'][int(next_command/15)%4]); next_command+=15
            elif args.scenario=='S3' and t>=next_command:
                # journal is a real blocking transfer, not a synthetic delay.
                if args.s3_handshake:
                    send(''); pump(.2)
                export_offset=len(data); export_started=time.monotonic()-start
                send('journal'); pump(.015)
                conn.write((b'status\n'*args.s3_status_count))
                commands.append({'host_elapsed_s':time.monotonic()-start,'ascii':'status\n'*args.s3_status_count})
                if args.s3_handshake:
                    deadline=time.monotonic()+2
                    while b'VG DONE op=journal ' not in data[export_offset:] and time.monotonic()<deadline:
                        pump(.05)
                    manifest['s3_export_attempts'].append({'host_started_s':export_started,
                        'done_observed':b'VG DONE op=journal ' in data[export_offset:],
                        'host_wait_finished_s':time.monotonic()-start})
                    pump(.2)
                next_command += rng.uniform(.75*args.s3_interval,1.25*args.s3_interval)
            elif args.scenario=='fault' and t>=next_command:
                if next_command==0:
                    send('mpu 69'); next_command=args.fault_nack_seconds
                else:
                    send('mpu 68'); next_command=float('inf')
            elif args.scenario=='boundary' and t>=next_command:
                level = min(4,int(t//15)); count = args.boundary_burst or [1,4,16,64,128][level]
                payload = b'status\n'*count
                conn.write(payload)
                commands.append({'host_elapsed_s':time.monotonic()-start,'ascii':payload.decode(),'boundary_level':level})
                next_command += 1
            pump(.1)
        pump(3); send(''); pump(.5)
        for attempt in range(3):
            send('stats'); pump(4)
            if metrics_complete(data): break
        else: raise RuntimeError('complete MCU metrics block not returned after workload')
        send('memory'); pump(.3); send('verify'); pump(1); send('durable'); pump(4)
        windows = re.findall(rb'elapsed_us=(\d+)',data)
        manifest['measurement_windows_us']=[int(w) for w in windows]
        manifest['endurance_adequate']=(args.scenario!='S2' or bool(windows and int(windows[-1])>=3600000000 and time.monotonic()-active>=3600))
        if args.scenario=='S2' and not args.allow_short and not manifest['endurance_adequate']:
            raise RuntimeError('S2 did not provide a full hour in its MCU measurement window')
        manifest['completed']=True
except BaseException as error:
    manifest['completed']=False; manifest['error']=repr(error)
    raise
finally:
    conn.close()
    manifest['elapsed_wall_seconds']=time.monotonic()-start
    if args.output.exists(): manifest['log_sha256']=hashlib.sha256(args.output.read_bytes()).hexdigest()
    manifest['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat()
    args.output.with_suffix('.manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
print(args.output)
