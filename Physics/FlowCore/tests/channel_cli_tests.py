import csv
import json
from pathlib import Path
import subprocess
import sys
import tempfile
exe,fixture,config=map(Path,sys.argv[1:])
def run(flow,channel,out):
    return subprocess.run([str(exe),str(flow),str(channel),str(out)],capture_output=True,text=True,timeout=90)
with tempfile.TemporaryDirectory() as temp:
    root=Path(temp)
    for name in ('a','b'):
        p=run(fixture,config,root/name);assert p.returncode==0,p.stderr
        data=json.loads((root/name/'result.json').read_text());assert data['passed'] and data['all_arrived'] and data['exited']==3
        assert data['accepted_steps']==600 and data['physical_time_s']==.3
        assert data['max_particle_mass_residual_kg']<1e-22 and data['max_exchange_residual_Ns']<1e-20
    for name in ('result.json','history.csv','transits.csv','input.flow','input.channel'):
        assert (root/'a'/name).read_bytes()==(root/'b'/name).read_bytes()
    events=list(csv.DictReader((root/'a'/'transits.csv').open()))
    assert all(r['status']=='exited' and 0<float(r['residence_time_s'])<.2 for r in events)
    assert float(events[1]['entry_time_s'])>.01
    history=list(csv.DictReader((root/'a'/'history.csv').open()));assert len(history)==600 and int(history[-1]['exited'])==3
    assert any(float(r['buffer_volume_m3'])>0 for r in history)
    old=(root/'a'/'result.json').read_bytes();assert run(fixture,config,root/'a').returncode==2 and (root/'a'/'result.json').read_bytes()==old
    for i,text in enumerate((config.read_text()+'garbage',config.read_text().replace('release 1 ','release -1 '),config.read_text().replace('reaction_enabled 1','reaction_enabled 2'),config.read_text().replace('coupling_substeps 20','coupling_substeps 0'),config.read_text().replace('ports_m 0.008','ports_m 0'),config.read_text().replace('release 2 ','release 1 '))):
        bad=root/f'bad{i}.channel';bad.write_text(text);out=root/f'invalid{i}'
        assert run(fixture,bad,out).returncode==2 and not out.exists()
    broken=root/'broken.flow';broken.write_text(fixture.read_text().replace('projection 20000','projection 1'))
    assert run(broken,config,root/'failed').returncode==3
    failure=json.loads((root/'failed'/'result.json').read_text());assert not failure['passed'] and failure['accepted_steps']==0 and failure['pending']==3 and failure['active']==0
    assert all(r['status']=='pending' and float(r['entry_time_s'])==-1 for r in csv.DictReader((root/'failed'/'transits.csv').open()))
    short=root/'short.flow';short.write_text(fixture.read_text().replace('0.0005 0.3','0.0005 0.03'))
    assert run(short,config,root/'partial').returncode==0
    partial=json.loads((root/'partial'/'result.json').read_text());assert partial['passed'] and not partial['all_arrived'] and partial['pending']+partial['active']==3 and partial['exited']==0
print('PASS channel CLI: full run, repeat, balances, delayed source, partial run, input rejection, rollback')
