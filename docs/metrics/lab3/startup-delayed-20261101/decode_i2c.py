import json
from pathlib import Path
clock=data=1;time=0; active=False; bits=[]; tx=[]; results=[]; start=0
for line in Path('startup-delay.vcd').open():
 s=line.strip()
 if s.startswith('#'): time=int(s[1:]);continue
 if len(s)!=2 or s[0] not in '01' or s[1] not in ('"','#'):continue
 v=int(s[0]);c=s[1]
 if c=='#':
  if clock and data==1 and v==0:
   if active and tx:results.append({'start_ns':start,'bytes':tx,'end':'repeated-start'})
   active=True;bits=[];tx=[];start=time
  elif clock and data==0 and v==1 and active:
   if tx:results.append({'start_ns':start,'bytes':tx,'end':'stop'})
   active=False;bits=[];tx=[]
  data=v
 else:
  if clock==0 and v==1 and active:
   bits.append(data)
   if len(bits)==9:
    b=0
    for bit in bits[:8]:b=(b<<1)|bit
    tx.append({'hex':f'{b:02x}','ack':bits[8]==0});bits=[]
  clock=v
Path('i2c-transactions.json').write_text(json.dumps(results,indent=2)+'\n')
for t in results[:35]:print('%.6f'%(t['start_ns']/1e9),' '.join(b['hex']+(' ACK' if b['ack'] else ' NACK') for b in t['bytes']),t['end'])
