#!/usr/bin/env python3
import re,sys
from pathlib import Path
files=list(Path(sys.argv[1]).glob('*.gcov'))
expected={'controller.c','event_log.c','impact.c','mpu6050.c'}
for name in sorted(expected):
    path=next(p for p in files if p.name==name+'.gcov')
    lines=executed=branches=taken=0
    for line in path.read_text().splitlines():
        match=re.match(r'\s*([^:]+):\s*(\d+):',line)
        if match and int(match[2]) and match[1].strip()!='-':
            lines+=1
            if not any(x in match[1] for x in ['#####','=====']):executed+=1
        match=re.match(r'branch\s+\d+\s+(.*)',line.strip())
        if match:
            branches+=1
            count=re.search(r'taken (\d+)',match[1])
            if count and int(count[1])>0:taken+=1
    line_pct=executed/lines*100;branch_pct=taken/branches*100
    assert line_pct>=90 and branch_pct>=80,(name,line_pct,branch_pct)
    print(f'PASS: {name}: lines={line_pct:.2f}% branch outcomes={branch_pct:.2f}%')
