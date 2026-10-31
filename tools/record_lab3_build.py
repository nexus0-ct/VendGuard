#!/usr/bin/env python3
"""Record actual build/check commands and hashes of the resulting VendGuard ELF."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root=Path(__file__).resolve().parents[1]
out=root/'docs/metrics/lab3/final-build'
out.mkdir(parents=True,exist_ok=True)
commands=[]
def run(name,args):
    started=datetime.datetime.now(datetime.timezone.utc).isoformat()
    result=subprocess.run(args,cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (out/name).write_bytes(result.stdout)
    commands.append({'args':args,'started_utc':started,'exit_code':result.returncode,'log':name})
    if result.returncode:raise RuntimeError(f'{name} failed with {result.returncode}')
def value(args):return subprocess.check_output(args,cwd=root,text=True).strip()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
run('build.log',['make','build'])
run('host.log',['make','test','sanitize'])
run('linux.log',['docker','run','--rm','--name','vendguard-lab3-final-verify-20261028',
    '-v',f'{root}:/workspace','-w','/workspace','vendguard-build:lab3','make','test','sanitize','coverage','coverage-lab3'])
run('coverage-thresholds.log',['python3','tools/check_coverage.py','tests/coverage/Linux-aarch64'])
run('memory-raw.log',['docker','run','--rm','--name','vendguard-lab3-final-size-20261028',
    '-v',f'{root}:/workspace','-w','/workspace','vendguard-build:lab3','sh','-c',
    'arm-none-eabi-size -A firmware/build/firmware.elf; arm-none-eabi-size firmware/build/*.o; arm-none-eabi-nm --size-sort -S --radix=d firmware/build/firmware.elf'])
run('memory-summary.log',['python3','tools/memory_lab3.py','--output',str(out/'memory.json')])
version=value(['python3','tools/source_id.py'])
source_files=[]
for folder in ['firmware','tests','tools','docker','.github']:
    source_files += [p for p in (root/folder).rglob('*') if p.is_file() and not {'build','coverage','__pycache__'}.intersection(p.relative_to(root).parts)]
source_files += [root/'diagram.json',root/'wokwi.toml',root/'Makefile']
artifacts=[root/'firmware/build'/('firmware.'+suffix) for suffix in ['elf','hex','bin','map']]
elf_hash=sha(artifacts[0])
snapshot=root.parents[2]/'Lab03/Work/firmware'/f'{version}-{elf_hash[:12]}'
snapshot.mkdir(parents=True,exist_ok=True)
for p in artifacts:shutil.copyfile(p,snapshot/p.name)
shutil.copyfile(root/'firmware/build/firmware.map',out/'firmware.map')
frames=[]
for p in (root/'firmware/build').glob('*.su'):
    for line in p.read_text().splitlines():
        function,amount,kind=line.split('\t');frames.append((int(amount),kind,function))
frames.sort(reverse=True)
(out/'static-stack.tsv').write_text('frame_bytes\tkind\tfunction\n'+''.join(f'{n}\t{k}\t{f}\n' for n,k,f in frames))
manifest={'recorded_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'build':version,
          'head':value(['git','rev-parse','HEAD']),'branch':value(['git','branch','--show-current']),
          'image_id':value(['docker','image','inspect','vendguard-build:lab3','--format','{{.Id}}']),
          'commands':commands,'source_files':{str(p.relative_to(root)):sha(p) for p in sorted(source_files)},
          'artifacts':{str(p.relative_to(root)):sha(p) for p in artifacts},'snapshot':str(snapshot),
          'notes':['HEAD is the Lab2 base. No commit or tag was created.',
                   'This manifest records build/host checks, not MCU or Wokwi execution.',
                   'Shared vendor HAL/CMSIS are included in hashes but excluded from application originality comparison.']}
(out/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
(snapshot/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'build':version,'elf_sha256':elf_hash,'snapshot':str(snapshot),'commands_passed':len(commands)},ensure_ascii=False))
