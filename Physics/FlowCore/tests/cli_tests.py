import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile

exe=Path(sys.argv[1]);fixtures=Path(sys.argv[2]);checks=0
def run(*args):
    return subprocess.run([str(exe),*[str(a) for a in args]],capture_output=True,text=True)
def finite_tree(value):
    if isinstance(value,dict):
        for v in value.values():finite_tree(v)
    elif isinstance(value,list):
        for v in value:finite_tree(v)
    elif isinstance(value,float):assert math.isfinite(value)
with tempfile.TemporaryDirectory(prefix='flow_contract_') as tmp:
    root=Path(tmp)
    for name in ('pressure_channel','velocity_channel','zero_drive'):
        result=run(fixtures/(name+'.flow'));assert result.returncode==0,result.stderr
        obj=json.loads(result.stdout);finite_tree(obj)
        assert obj['contract_valid'] is True and obj['flow_evolved'] is False and obj['solver_implemented'] is True
        assert obj['units']=='SI' and obj['grid']['p_cells']==1024
        assert obj['result_kind']=='input_contract_and_analytic_estimates_only'
        assert obj['steps']==10000 and obj['combined_budget']<=.5
        if name=='zero_drive':assert obj['analytic_mass_flow_kg_s']==0
        else:assert abs(obj['analytic_mean_m_s']-.02)<1e-9
        again=run(fixtures/(name+'.flow'));assert result.stdout==again.stdout
        out=root/(name+'.json');written=run(fixtures/(name+'.flow'),out);assert written.returncode==0,written.stderr
        assert out.read_text()==result.stdout
        before=out.read_bytes();assert run(fixtures/(name+'.flow'),out).returncode!=0 and out.read_bytes()==before
        checks+=1
    assert run(fixtures/'unsupported_vacuum.flow').returncode!=0;checks+=1
    base=(fixtures/'pressure_channel.flow').read_text()
    malformed=[base.replace('units SI','units cm'),base.replace('FLOW_CONTRACT 1','FLOW_CONTRACT 2'),
               base.replace('parallel_plates','circular_pipe'),base.replace('planar_isothermal_laminar','compressible'),
               base.replace('grid 64 16','grid -64 16'),base.replace('grid 64 16','grid 99999999999999999999999 16'),
               base.replace('293.15','nan'),base.replace('293.15','300'),base.replace('101325.004344','inf'),
               base.replace('right pressure_absolute_pa 101325','right pressure_gauge_pa -35000'),
               base.replace('right pressure_absolute_pa 101325','right no_slip 0'),
               base.replace('top no_slip 0','top no_slip 1'),base.replace('0.0005 5','0.01 5'),
               base.replace('0.0005 5','0.0005 5.0001'),base+'dt 1\n',base.replace('end\n',''),
               'x'*65537]
    for i,text in enumerate(malformed):
        path=root/f'bad_{i}.flow';path.write_text(text);out=root/f'bad_{i}.json'
        result=run(path,out);assert result.returncode!=0,(i,result.stdout)
        assert not out.exists() and not out.with_suffix('.json.tmp').exists()
        checks+=1
print(f'PASS {checks} CLI cases: valid fixtures, finite reports, exact repeat, overwrite protection and invalid contracts')
