from pathlib import Path
import importlib.util,json,copy,sys
r=Path(sys.argv[1]);spec=importlib.util.spec_from_file_location('soft08',r/'Scripts/run_optimization08_validation.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
c=json.loads((r/'Physics/PowderCore/fixtures/optimization08_contract.json').read_text())
s={'case':'packing32','error':'','accepted_steps':'100','requested_steps':'100','max_overlap_ratio':'.01','mass_kg':'1','radius_m':'.02','duration_s':'.04','max_mass_error':'0','contact_integral_s':'1','mixing_valid':'1','outside':'0','arrived_count':'0','count':'32','arrived_mass_kg':'0','mean_residence_s':'0','p95_residence_s':'0'}
series=[]
for i in range(21):series.append({'sample':str(i),'time_s':str(i*.002),'com_z':'0','energy_j':'0','wall_work_j':'0','torque_impulse_z':'0','mixing_index':'0','mixing_valid':'1','outside':'0','arrived_mass_kg':'0','arrived_count':'0','active_mass_kg':'1'})
a={'summary':s,'series':series,'events':[],'states':[],'materials':[]}
assert m.compare(a,a,c,'integration')['accepted'] and m.compare(a,a,c,'surrogate')['accepted']
for key in ['com_z','energy_j','wall_work_j','torque_impulse_z']:
 b=copy.deepcopy(a);b['series'][10][key]='1';assert not m.compare(b,a,c,'surrogate')['accepted']
for key,val in [('max_overlap_ratio','.05'),('max_mass_error','.1'),('error','guard'),('contact_integral_s','0')]:
 b=copy.deepcopy(a);b['summary'][key]=val;assert not m.compare(b,a,c,'surrogate')['accepted']
b=copy.deepcopy(a);b['series'].pop();assert not m.compare(b,a,c,'integration')['accepted']
b=copy.deepcopy(a);b['series'][10]['energy_j']='nan'
try:m.compare(b,a,c,'surrogate');raise AssertionError('nan accepted')
except ValueError:pass
b=copy.deepcopy(a);b['summary']['case']='mixing5_long';b['series'][10]['mixing_index']='.2';assert not m.compare(b,a,c,'surrogate')['accepted']
b=copy.deepcopy(a);b['summary'].update(case='discharge_wide',arrived_count='32');assert not m.compare(b,a,c,'surrogate')['accepted']
# Bad physical reference is ineligible even when the same failure occurs in the candidate.
b=copy.deepcopy(a);b['summary']['max_overlap_ratio']='.06';assert not m.compare(b,b,c,'surrogate')['accepted']
print('PASS softening comparator: physical-reference failure, bulk metrics, overlap, conservation, mixing, missing outlet events, nonfinite values')
