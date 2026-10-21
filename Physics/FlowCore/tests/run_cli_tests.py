import json
import subprocess
import sys
import tempfile
from pathlib import Path

exe,fixtures=Path(sys.argv[1]),Path(sys.argv[2])
def run(*args):
    return subprocess.run([str(exe),*[str(a) for a in args]],capture_output=True,text=True,timeout=60)
with tempfile.TemporaryDirectory() as folder:
    root=Path(folder)
    for name in ('zero_drive','pressure_channel','velocity_channel'):
        source=root/(name+'.flow')
        source.write_text((fixtures/(name+'.flow')).read_text().replace('0.0005 5','0.0005 0.005'))
        a,b=root/(name+'_a'),root/(name+'_b')
        for output in (a,b):
            result=run(source,output);assert result.returncode==0,result.stderr
            obj=json.loads((output/'result.json').read_text())
            assert obj['passed'] and obj['solver_implemented'] and obj['accepted_steps']==10 and obj['physical_time_s']==.005
            assert obj['max_divergence_linf_s_inv']<=1e-7 and obj['max_mass_balance_relative']<=1e-8
            assert len((output/'history.csv').read_text().splitlines())==11
        for file in ('last_accepted_fields.csv','history.csv','input.flow','contract.json'):
            assert (a/file).read_bytes()==(b/file).read_bytes(),file
        before=(a/'result.json').read_bytes()
        assert run(source,a).returncode==2 and (a/'result.json').read_bytes()==before
    invalid=root/'bad'
    assert run(fixtures/'unsupported_vacuum.flow',invalid).returncode==2 and not invalid.exists()
    failed_input=root/'fail.flow'
    failed_input.write_text((root/'velocity_channel.flow').read_text().replace('projection 20000','projection 1'))
    output=root/'failed';assert run(failed_input,output).returncode==3
    obj=json.loads((output/'result.json').read_text());assert not obj['passed'] and obj['accepted_steps']==0 and obj['physical_time_s']==0
    assert len((output/'history.csv').read_text().splitlines())==1
print('PASS flow_run: computed fields, repeatability, all-step history, overwrite/invalid rejection, failed-step clock')
