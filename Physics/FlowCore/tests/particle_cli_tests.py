import json
from pathlib import Path
import subprocess
import sys
import tempfile
exe,fixture=Path(sys.argv[1]),Path(sys.argv[2])
def run(*args):return subprocess.run([str(exe),*[str(x) for x in args]],capture_output=True,text=True,timeout=60)
with tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp)
    for name in ('a','b'):
        result=run(fixture,root/name);assert result.returncode==0,result.stderr
        data=json.loads((root/name/'result.json').read_text());assert data['passed'] and not data['air_reaction_enabled'] and data['physical_time_s']==.05 and data['accepted_air_steps']==100
        assert 0<data['max_particle_Re']<=.1 and data['max_dt_over_tau']<=.05
        assert len((root/name/'particles.csv').read_text().splitlines())==102
    for name in ('result.json','particles.csv','particle.json','input.flow'):
        assert (root/'a'/name).read_bytes()==(root/'b'/name).read_bytes()
    original=(root/'a'/'result.json').read_bytes();assert run(fixture,root/'a').returncode==2 and (root/'a'/'result.json').read_bytes()==original
    for args in (('.04','1000','20'),('0.000001','1000','20'),('0.00003','1000','1'),('nan','1000','20'),('0.00003','1000','2.5')):
        output=root/'invalid';assert run(fixture,output,*args).returncode==2 and not output.exists()
    long=root/'long.flow';long.write_text(fixture.read_text().replace('0.0005 0.05','0.0005 1'))
    result=run(long,root/'failed');assert result.returncode==3,result.stderr
    data=json.loads((root/'failed'/'result.json').read_text());assert not data['passed'] and 0<data['physical_time_s']<1
    lines=(root/'failed'/'particles.csv').read_text().splitlines();assert len(lines)==data['accepted_air_steps']+2
print('PASS one-way CLI repeat, settings validation, history, overwrite protection and bounded-domain failure')
