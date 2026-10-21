import json
from pathlib import Path
import subprocess
import sys
import tempfile
exe,fixture=Path(sys.argv[1]),Path(sys.argv[2])
def run(*args):return subprocess.run([str(exe),*[str(x) for x in args]],capture_output=True,text=True,timeout=60)
with tempfile.TemporaryDirectory() as temp:
    root=Path(temp)
    for name in ('a','b'):
        r=run(fixture,root/name);assert r.returncode==0,r.stderr
        data=json.loads((root/name/'result.json').read_text());assert data['passed'] and data['reaction_enabled'] and not data['finite_porosity_equations']
        assert data['accepted_steps']==20 and data['physical_time_s']==.01 and data['max_solid_fraction']<=.001
        assert abs(data['mapped_volume_m3']/data['particle_volume_m3']-1)<1e-12 and data['max_exchange_error_Ns']<1e-20
    for file in ('result.json','exchange.csv','volume_pressure.csv','air_velocity.csv','coupling.json','input.flow'):
        assert (root/'a'/file).read_bytes()==(root/'b'/file).read_bytes()
    assert len((root/'a'/'exchange.csv').read_text().splitlines())==21
    old=(root/'a'/'result.json').read_bytes();assert run(fixture,root/'a').returncode==2 and (root/'a'/'result.json').read_bytes()==old
    r=run(fixture,root/'off',20,0);assert r.returncode==0,r.stderr
    off=json.loads((root/'off'/'result.json').read_text());assert not off['reaction_enabled'] and off['final_particle_vx_m_s']>data['final_particle_vx_m_s']
    assert (root/'off'/'air_velocity.csv').read_bytes()!=(root/'a'/'air_velocity.csv').read_bytes()
    for args in ((1,),('nan',),(20,2)):
        assert run(fixture,root/'invalid',*args).returncode==2 and not (root/'invalid').exists()
    bad=root/'bad.flow';bad.write_text(fixture.read_text().replace('projection 20000','projection 1'))
    assert run(bad,root/'failed').returncode==3
    failure=json.loads((root/'failed'/'result.json').read_text());assert not failure['passed'] and failure['accepted_steps']==0 and failure['physical_time_s']==0
print('PASS two-way CLI: conservative volumes, active reaction, exact repeat, controls, invalid inputs, transactional failure')
