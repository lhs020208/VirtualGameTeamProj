from pathlib import Path
import argparse,csv,json,math,subprocess,hashlib

def read(p):
 with p.open(encoding='utf-8-sig',newline='') as f:return list(csv.DictReader(f))
def number(row,k):
 v=float(row[k])
 if not math.isfinite(v):raise ValueError("nonfinite metric: "+k)
 return v
def safe(run,contract):
 limits=contract['safety']
 s=run['summary'];fail=[]
 if s['error'] or int(s['accepted_steps'])!=int(s['requested_steps']):fail.append('solver_rejection')
 if number(s,'max_overlap_ratio')>=limits['overlap_radius_ratio_max_exclusive']:fail.append('overlap_5pct')
 if s['case'].startswith('mixing') and (s['mixing_valid']!='1' or int(s['outside'])):fail.append('invalid_mixing_region')
 if s['case'].startswith('collision'):
  if number(s,'peak_energy_j')>number(s,'initial_energy_j')*(1+limits['collision_energy_growth_relative'])+1e-15:fail.append('energy_growth')
  if number(s,'max_momentum_residual')>number(s,'mass_kg')*.1*limits['collision_momentum_residual_relative_mass_times_0_1_m_s']:fail.append('momentum')
 return fail
def compare(a,b,contract):
 v=contract['convergence'];fail=safe(a,contract);s=a['summary'];r=b['summary'];radius=number(r,'radius_m');mass=number(r,'mass_kg');scale=mass*9.81*radius
 metrics={}
 if len(a['states'])!=len(b['states']) or len(a['series'])!=len(b['series']):return {'accepted':False,'failures':fail+['incomplete_samples'],'metrics':metrics}
 for label,keys,normal,tol in [('position',['x','y','z'],radius,v['position_max_norm_div_radius']),('velocity',['vx','vy','vz'],.1,v['velocity_max_norm_div_0_1_m_s']),('omega',['wx','wy','wz'],.1/radius,v['omega_max_norm_div_0_1_over_radius'])]:
  peak=0
  for x,y in zip(a['states'],b['states']):
   if x['sample']!=y['sample'] or x['id']!=y['id']:raise RuntimeError('mismatched sample identities')
   peak=max(peak,math.sqrt(sum((number(x,k)-number(y,k))**2 for k in keys))/normal)
  metrics[label+'_max_normalized']=peak
  if peak>tol:fail.append(label)
 for key,floor,rel in [('energy_j',scale*v['energy_absolute_scale_mass_g_radius'],v['energy_relative']),('wall_work_j',scale*v['work_absolute_scale_mass_g_radius'],v['work_relative']),('torque_impulse_z',mass*radius*.1*v['torque_impulse_absolute_scale_mass_radius_0_1'],v['torque_impulse_relative']),('contact_integral_s',number(r,'duration_s')*v['contact_time_absolute_scale_duration'],v['contact_time_relative'])]:
  peak=max(abs(number(v,key)) for v in b['series']);diff=max(abs(number(x,key)-number(y,key)) for x,y in zip(a['series'],b['series']))
  limit=floor+rel*peak;metrics[key+'_error']=diff;metrics[key+'_tolerance']=limit
  if diff>limit:fail.append(key)
 overlap=abs(number(s,'max_overlap_ratio')-number(r,'max_overlap_ratio'));metrics['overlap_error']=overlap
 if overlap>v['overlap_ratio_abs']+v['overlap_ratio_relative']*number(r,'max_overlap_ratio'):fail.append('overlap_convergence')
 if s['case'].startswith('collision'):
  diff=abs(number(s,'peak_force_n')-number(r,'peak_force_n'));metrics['peak_force_error']=diff
  if diff>v['collision_peak_force_abs_n']+v['collision_peak_force_relative']*number(r,'peak_force_n'):fail.append('peak_force')
 if s['case'].startswith('mixing'):
  diff=max(abs(number(x,'mixing_index')-number(y,'mixing_index')) for x,y in zip(a['series'],b['series']));metrics['mixing_index_error']=diff
  if diff>v['mixing_index_abs']:fail.append('mixing_index')
  if any(x['mixing_valid']!=y['mixing_valid'] or x['outside']!=y['outside'] for x,y in zip(a['series'],b['series'])):fail.append('mixing_discrete_status')
 return {'accepted':not fail,'failures':fail,'metrics':metrics}
def main():
 root=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--exe',type=Path,default=root/'Intermediate/PowderCore/Release/powder_dt07.exe');p.add_argument('--out',type=Path,default=root/'Saved/Optimization07_validation');args=p.parse_args();args.out.mkdir(parents=True,exist_ok=False)
 contract=json.loads((root/'Physics/PowderCore/fixtures/optimization07_contract.json').read_text(encoding='utf-8'));(args.out/'contract.json').write_text(json.dumps(contract,indent=2),encoding='utf-8')
 report={'status':'running','contract':contract,'cases':{},'automatic_default_change':False,'editor_fixed_dt_s':2e-6,'mm_baseline_dt_s':2e-8}
 for case in contract['cases']:
  base=2e-8 if case=='mm_packing32' else 2e-6;runs=[]
  for i,factor in enumerate(contract['factors']):
   folder=args.out/(case+'_'+str(i));result=subprocess.run([str(args.exe),case,format(base*factor,'.17g'),str(folder)],capture_output=True,text=True,timeout=180)
   (args.out/(case+'_'+str(i)+'.log')).write_text(result.stdout+result.stderr,encoding='utf-8');print(result.stdout.strip(),flush=True)
   if result.returncode:raise RuntimeError('sweep execution failed '+str(folder))
   runs.append({'factor':factor,'summary':read(folder/'summary.csv')[0],'states':read(folder/'states.csv'),'series':read(folder/'series.csv')})
  reference=runs[0];reference_gate=compare(runs[1],reference,contract);ref_safety=safe(reference,contract)
  evaluations=[]
  for run in runs:
   comparison=compare(run,reference,contract);comparison.update({'factor':run['factor'],'summary':run['summary']});evaluations.append(comparison)
  eligible=[e for e in evaluations[2:] if e['accepted']] if reference_gate['accepted'] and not ref_safety else []
  selected=max(eligible,key=lambda e:float(e['summary']['dt_s'])) if eligible else None
  report['cases'][case]={'reference_safety_failures':ref_safety,'reference_halfstep_gate':reference_gate,'runs':evaluations,'largest_tested_passing_dt_s':float(selected['summary']['dt_s']) if selected else None,'scope':'only this unchanged fixture and tested duration; not general production authorization'}
  (args.out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
 report['status']='completed'
 report['exe_sha256']=hashlib.sha256(args.exe.read_bytes()).hexdigest()
 sources=[*list((root/'Physics/PowderCore/include/powder').glob('*.hpp')),*list((root/'Physics/PowderCore/src').glob('*.cpp')),root/'Physics/PowderCore/app/timestep_sweep.cpp',Path(__file__)]
 report['source_sha256']={f.relative_to(root).as_posix():hashlib.sha256(f.read_bytes()).hexdigest() for f in sources}
 (args.out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print('COMPLETED; defaults unchanged; '+str(args.out/'report.json'))
if __name__=='__main__':main()
