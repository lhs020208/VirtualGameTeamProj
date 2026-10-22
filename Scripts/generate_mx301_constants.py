from pathlib import Path
import json
r=Path('D:/GitHub/VirtualGameTeamProj');data=json.loads((r/'SourceAssets/Granular/MX301_convex.json').read_text())
assert len(data['interior'])==1 and max(p['convex_violation'] for p in data['interior']+data['rotor'])<3e-5
planes=data['interior'][0]['planes'];parts=[]
for p in data['rotor']:
 start=len(planes);planes+=p['planes'];parts.append([start,len(p['planes']),*[p['bounds'][k][0] for k in range(3)],*[p['bounds'][k][1] for k in range(3)]])
def f(x):return f'{float(x):.9e}f'
cpu='// Generated from non-final teamproj3.blend; local SI coordinates.\nstatic const float mxplanes[][4]={\n'+',\n'.join('{'+','.join(map(f,p))+'}' for p in planes)+'\n};\nstatic const float mxparts[][8]={\n'+',\n'.join('{'+','.join(map(f,p))+'}' for p in parts)+'\n};\n'
cl='__constant float4 mxplanes[]={\n'+',\n'.join('(float4)('+','.join(map(f,p))+')' for p in planes)+'\n};\n__constant float8 mxparts[]={\n'+',\n'.join('(float8)('+','.join(map(f,p))+')' for p in parts)+'\n};\n'
(r/'Physics/PowderCore/src/granular_mx301_cpu.inc').write_text(cpu)
(r/'Physics/PowderCore/src/granular_mx301_cl.inc').write_text('R"CLC(\n'+cl+')CLC"\n')
from pathlib import Path
p=Path('D:/GitHub/VirtualGameTeamProj/Physics/PowderCore/src/granular_mx301_cl.inc')
s=p.read_text();assert s.startswith('R"CLC(\n') and s.endswith(')CLC"\n');s=s[6:-6]
p.write_text(''.join('R"CLC('+s[i:i+10000]+')CLC"\n' for i in range(0,len(s),10000)))
