import sys,hashlib,json
from pathlib import Path
src,dst,begin,end=sys.argv[1:];begin=int(float(begin)*1e9);end=int(float(end)*1e9)
s=Path(src).read_text();head,body=s.split('$enddefinitions $end',1)
head+='$comment '+json.dumps({'source_sha256':hashlib.sha256(s.encode()).hexdigest(),'begin_ns':begin,'end_ns':end,'operation':'crop original transitions and subtract begin_ns'})+' $end\n$enddefinitions $end\n'
states={};out=[];t=0;started=False
for line in body.splitlines():
 if line.startswith('#'):
  t=int(line[1:])
  if t>end:break
  if t>=begin:
   if not started:
    out+=['#0']+[v+k for k,v in states.items()];started=True
   out.append('#'+str(t-begin))
 elif len(line)>1 and line[0] in '01xz':
  states[line[1:]]=line[0]
  if t>=begin:out.append(line)
# Keep the observation end so VCD readers flush the final transition.
out.append('#'+str(end-begin))
Path(dst).write_text(head+'\n'.join(out)+'\n')
