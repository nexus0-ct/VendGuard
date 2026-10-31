#!/usr/bin/env python3
"""Capture a real Wokwi startup over the existing RFC2217 port, before UI Start."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import re
import subprocess
import time
import serial

p=argparse.ArgumentParser()
p.add_argument('--output',type=Path,required=True)
p.add_argument('--seconds',type=float,default=4)
p.add_argument('--port',type=int,default=4136)
a=p.parse_args()
if a.seconds<3:p.error('capture at least three seconds after first UART byte')
sidecar=a.output.with_suffix('.manifest.json')
if a.output.exists() or sidecar.exists():p.error('refusing overwrite')
root=Path(__file__).resolve().parents[1]
build=subprocess.check_output(['python3',str(root/'tools/source_id.py')],text=True).strip()
record={'started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'build_expected':build,'elf_sha256':hashlib.sha256((root/'firmware/build/firmware.elf').read_bytes()).hexdigest(),
        'runner_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'note':'Raw UART capture. Operator starts own simulation through UI; this script does not reset or emulate an MCU.',
        'commands':[],'connection_errors':[]}
a.output.parent.mkdir(parents=True,exist_ok=True)
connection=None;data=bytearray();first=None;sent=False;begin=time.monotonic()
try:
    while connection is None and time.monotonic()-begin<40:
        try:connection=serial.serial_for_url(f'rfc2217://localhost:{a.port}',baudrate=115200,timeout=.05)
        except serial.SerialException as e:
            record['connection_errors'].append(str(e));time.sleep(.2)
    if connection is None:raise TimeoutError('RFC2217 not available within forty wall seconds')
    with a.output.open('wb') as raw:
        while first is None or time.monotonic()-first<a.seconds:
            if first is None and time.monotonic()-begin>40:raise TimeoutError('no UART startup bytes')
            chunk=connection.read(4096)
            if chunk:
                if first is None:first=time.monotonic()
                raw.write(chunk);raw.flush();data.extend(chunk)
            if first is not None and not sent and time.monotonic()-first>=2:
                payload='version\nstatus\nmemory\nverify\n'
                connection.write(payload.encode('ascii'));sent=True
                record['commands'].append({'host_elapsed_s':time.monotonic()-begin,'ascii':payload})
    if f'VG VERSION build={build}'.encode() not in data:raise RuntimeError('startup firmware version missing or different')
    record['completed']=True
except BaseException as e:
    record['completed']=False;record['error']=repr(e);raise
finally:
    if connection is not None:connection.close()
    record['elapsed_wall_s']=time.monotonic()-begin
    record['boot_seen']=b'VG BOOT ' in data
    record['before_uart_heap']=[{'arena':int(x),'used':int(y)} for x,y in re.findall(rb'MEM_STAGE stage=before_uart heap_arena=(\d+) heap_used=(\d+)',data)]
    record['first_clock_us']={d.decode():int(t) for d,t in re.findall(rb'VG (?:CHECK|FIRST) device=(LDR_ADC|PIR|MPU6050)[^\r\n]*t_us=(\d+)',data)}
    record['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat()
    if a.output.exists():record['raw_sha256']=hashlib.sha256(a.output.read_bytes()).hexdigest()
    sidecar.write_text(json.dumps(record,indent=2)+'\n')
print(a.output)
