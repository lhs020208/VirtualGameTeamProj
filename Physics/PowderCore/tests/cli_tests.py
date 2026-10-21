"""Standard-library integration checks; executable path is the sole argument."""
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile

exe=str(Path(sys.argv[1]).resolve())
def run(*args):
    return subprocess.run([exe,*args],capture_output=True,text=True)
with tempfile.TemporaryDirectory() as tmp:
    out=Path(tmp)/'record.json'
    args=['--diameter','0.002','--density','2500','--dt','0.0001','--steps','10000','--seed','42','--output',str(out)]
    a=run(*args)
    assert a.returncode==0,a.stderr
    data=json.loads(a.stdout)
    assert data==json.loads(out.read_text())
    assert data['seed']==42 and data['completed_steps']==10000
    assert data['simulation_time_s']==1 and data['randomness_used'] is False
    # Tabulated sphere values (diameter 2mm, density 2500kg/m3).
    assert math.isclose(data['particle']['mass_kg'],1.0471975511965977e-5,rel_tol=1e-12)
    assert math.isclose(data['particle']['inertia_kg_m2'],4.188790204786391e-12,rel_tol=1e-12)
    assert run(*args).stdout==a.stdout
    invalid=[['--dt','0'],['--dt','nan'],['--dt','inf'],['--density','-1'],['--diameter','0'],['--diameter','1e200'],['--steps','-1'],['--steps','1.5'],['--steps','10000001'],['--seed','18446744073709551616'],['--dt','0.1oops'],['--unknown','1'],['--dt'],['--dt','1','--dt','2']]
    for bad in invalid:
        result=run(*bad)
        assert result.returncode!=0 and 'ERROR:' in result.stderr,(bad,result)
    before=out.read_text()
    assert run('--dt','-1','--output',str(out)).returncode!=0
    assert out.read_text()==before
    assert run('--output',str(Path(tmp)/'missing'/'x.json')).returncode!=0
    print('PASS CLI: JSON roundtrip, reproducibility, sphere reference, 14 invalid arguments, output preservation and write failure')
