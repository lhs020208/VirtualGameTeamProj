from pathlib import Path
import copy,importlib.util,json,sys
root=Path(sys.argv[1]);spec=importlib.util.spec_from_file_location('dt_runner',root/'Scripts/run_optimization07_validation.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
c=json.loads((root/'Physics/PowderCore/fixtures/optimization07_contract.json').read_text())
s={k:'0' for k in ['max_overlap_ratio','contact_integral_s','wall_work_j','torque_impulse_z','peak_force_n','initial_energy_j','peak_energy_j','max_momentum_residual','outside']}
s.update(case='packing32',error='',accepted_steps='100',requested_steps='100',radius_m='.02',mass_kg='1',duration_s='.04',mixing_valid='1')
state={k:'0' for k in ['x','y','z','vx','vy','vz','wx','wy','wz']};state.update(sample='1',id='1')
series={k:'0' for k in ['energy_j','wall_work_j','torque_impulse_z','contact_integral_s','mixing_index','outside']};series['mixing_valid']='1'
a={'summary':s,'states':[state],'series':[series]}
assert m.compare(a,a,c)['accepted']
for key,val in [('x','.001'),('vx','.01'),('wx','1')]:
 b=copy.deepcopy(a);b['states'][0][key]=val;assert not m.compare(b,a,c)['accepted']
for key in ['energy_j','wall_work_j','torque_impulse_z','contact_integral_s']:
 b=copy.deepcopy(a);b['series'][0][key]='1';assert not m.compare(b,a,c)['accepted']
b=copy.deepcopy(a);b['summary']['error']='guard';assert not m.compare(b,a,c)['accepted']
b=copy.deepcopy(a);b['summary']['max_overlap_ratio']='.05';assert not m.compare(b,a,c)['accepted']
b=copy.deepcopy(a);b['states']=[];assert not m.compare(b,a,c)['accepted']
b=copy.deepcopy(a);b['states'][0]['x']='nan'
try:m.compare(b,a,c);raise AssertionError('accepted nan')
except ValueError:pass
b=copy.deepcopy(a);b['summary']['case']='mixing5_short';b['series'][0]['mixing_index']='.02';assert not m.compare(b,a,c)['accepted']
print('PASS convergence comparator rejects state/metric/status/missing samples/nonfinite regressions')
