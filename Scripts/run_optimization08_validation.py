from pathlib import Path
import argparse,csv,json,math,subprocess,hashlib,shutil

def rows(p):
 with p.open(encoding='utf-8-sig',newline='') as f:return list(csv.DictReader(f))
def num(row,k):
 v=float(row[k])
 if not math.isfinite(v):raise ValueError('nonfinite '+k)
 return v
def load(path):return {'summary':rows(path/'summary.csv')[0],'series':rows(path/'series.csv'),'events':rows(path/'events.csv'),'states':rows(path/'states.csv'),'materials':rows(path/'materials.csv')}
def safety(a,c):
 s=a['summary'];lim=c['safety'];bad=[]
 if s['error'] or int(s['accepted_steps'])!=int(s['requested_steps']):bad.append('solver_rejection')
 if num(s,'max_overlap_ratio')>=lim['max_overlap_ratio_exclusive']:bad.append('overlap_5pct')
 if num(s,'max_mass_error')>lim['mass_relative']*num(s,'mass_kg')+lim['mass_absolute_kg']:bad.append('mass_balance')
 if lim['require_contacts'] and num(s,'contact_integral_s')<=0:bad.append('no_contact_evidence')
 if s['mixing_valid']!='1' or int(s['outside']):bad.append('invalid_mixing')
 if s['case']=='discharge_wide':
  if lim['require_all_discharge'] and int(s['arrived_count'])!=int(s['count']):bad.append('incomplete_discharge')
  if len(a['events'])!=int(s['arrived_count']) or len(set(x['id'] for x in a['events']))!=len(a['events']):bad.append('event_count_or_duplicate')
  if any(num(e,'crossing_s')<0 or num(e,'crossing_s')>num(s,'duration_s') or num(e,'mass_kg')<=0 or e['group'] not in {'0','1'} for e in a['events']):bad.append('invalid_outlet_event')
  mass=sum(num(e,'mass_kg') for e in a['events'])
  if abs(mass-num(s,'arrived_mass_kg'))>1e-12*num(s,'mass_kg')+1e-18:bad.append('event_mass')
 return bad
def compare(a,b,c,kind):
 bad=safety(a,c);bad+=['reference_'+x for x in safety(b,c)];v=c[kind];s=a['summary'];ref=b['summary'];metrics={}
 if len(a['series'])!=21 or len(b['series'])!=21:return {'accepted':False,'failures':bad+['incomplete_samples'],'metrics':metrics}
 for x,y in zip(a['series'],b['series']):
  if x['sample']!=y['sample'] or abs(num(x,'time_s')-num(y,'time_s'))>1e-12:raise ValueError('unmatched physical samples')
 mass=num(ref,'mass_kg');radius=num(ref,'radius_m');duration=num(ref,'duration_s');flow=s['case']=='discharge_wide'
 if not flow:
  e=max(abs(num(x,'com_z')-num(y,'com_z')) for x,y in zip(a['series'],b['series']))/radius;metrics['com_error_radius']=e
  if e>v['com_radius_fraction']:bad.append('com_height')
  for key,floor in [('energy_j',mass*9.81*radius*c['floors']['energy_mass_g_radius']),('wall_work_j',mass*9.81*radius*c['floors']['energy_mass_g_radius']),('torque_impulse_z',mass*radius*.1*c['floors']['torque_impulse_mass_radius_0_1'])]:
   peak=max(abs(num(y,key)) for y in b['series']);e=max(abs(num(x,key)-num(y,key)) for x,y in zip(a['series'],b['series']));limit=floor+v['bulk_relative']*peak;metrics[key]={'error':e,'tolerance':limit}
   if e>limit:bad.append(key)
 e=abs(num(s,'max_overlap_ratio')-num(ref,'max_overlap_ratio')) if kind=='integration' else num(s,'max_overlap_ratio')-num(ref,'max_overlap_ratio')
 metrics['overlap_delta']=e
 if e>v['overlap_abs' if kind=='integration' else 'overlap_increase_abs']:bad.append('overlap_change')
 e=abs(num(s,'contact_integral_s')-num(ref,'contact_integral_s'));limit=c['floors']['contact_duration']*duration+v['bulk_relative']*abs(num(ref,'contact_integral_s'));metrics['contact_time']={'error':e,'tolerance':limit}
 if e>limit:bad.append('contact_duration')
 if s['case']=='mixing5_long':
  e=max(abs(num(x,'mixing_index')-num(y,'mixing_index')) for x,y in zip(a['series'],b['series']));metrics['mixing_index_error']=e
  if e>v['mixing_abs']:bad.append('mixing_index')
  if any(x['mixing_valid']!=y['mixing_valid'] or x['outside']!=y['outside'] for x,y in zip(a['series'],b['series'])):bad.append('mixing_status')
 if flow:
  e=max(abs(num(x,'arrived_mass_kg')-num(y,'arrived_mass_kg'))/mass for x,y in zip(a['series'],b['series']));metrics['outflow_fraction_error']=e
  if e>v['discharge_fraction_abs']:bad.append('outflow_curve')
  for key in ['mean_residence_s','p95_residence_s']:
   e=abs(num(s,key)-num(ref,key));limit=v['residence_absolute_s']+v['residence_relative']*num(ref,key);metrics[key]={'error':e,'tolerance':limit}
   if e>limit:bad.append(key)
  left={e['id']:e for e in a['events']};right={e['id']:e for e in b['events']}
  if left.keys()!=right.keys():bad.append('outlet_ids')
  elif left:
   e=max(abs(num(left[i],'crossing_s')-num(right[i],'crossing_s')) for i in left);limit=v['residence_absolute_s']+v['residence_relative']*num(ref,'p95_residence_s');metrics['individual_residence_error']=e
   if e>limit:bad.append('individual_residence')
 return {'accepted':not bad,'failures':sorted(set(bad)),'metrics':metrics}
def material_check(a,alpha):
 if len(a['materials'])!=5 or {m['role'] for m in a['materials']}!={'pair00','pair01','pair11','wall0','wall1'}:raise ValueError('missing material roles')
 for m in a['materials']:
  if m['scope']!='bulk_wide_flow' or num(m,'stiffness_ratio')!=alpha:raise ValueError('material provenance')
  for key,factor in [('kn',alpha),('kt',alpha),('cn',math.sqrt(alpha)),('ct',math.sqrt(alpha)),('mu',1),('young_a',1)]:
   if num(m,'computed_'+key)!=num(m,'physical_'+key)*factor:raise ValueError('inconsistent material scale '+key)
def main():
 root=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--exe',type=Path,default=root/'Intermediate/PowderCore/Release/powder_soft08.exe');p.add_argument('--reuse',type=Path);p.add_argument('--out',type=Path,default=root/'Saved/Optimization08_validation');args=p.parse_args();args.out.mkdir(parents=True,exist_ok=False)
 c=json.loads((root/'Physics/PowderCore/fixtures/optimization08_contract.json').read_text(encoding='utf-8'));(args.out/'contract.json').write_text(json.dumps(c,indent=2),encoding='utf-8')
 reuse=None
 if args.reuse:
  reuse=json.loads((args.reuse/'report.json').read_text(encoding='utf-8'))
  if reuse['status']!='completed' or reuse['exe_sha256']!=hashlib.sha256(args.exe.read_bytes()).hexdigest():raise ValueError('reuse executable mismatch')
 report={'status':'running','contract':c,'cases':{},'runtime_changed':False};common=None
 for case in c['cases']:
  base=c['mm_baseline_dt_s'] if case=='mm_packing32' else c['baseline_dt_s'];runs={}
  for ai,alpha in enumerate(c['alphas']):
   for di,factor in enumerate(c['dt_factors']):
    folder=args.out/f'{case}_a{ai}_d{di}'
    if reuse and case in reuse['contract']['cases'] and alpha in reuse['contract']['alphas'] and factor in reuse['contract']['dt_factors']:
     old=args.reuse/f"{case}_a{reuse['contract']['alphas'].index(alpha)}_d{reuse['contract']['dt_factors'].index(factor)}";shutil.copytree(old,folder)
     (args.out/(folder.name+'.log')).write_text('REUSED unchanged executable/input: '+str(old)+'\n',encoding='utf-8')
    else:
     result=subprocess.run([str(args.exe),case,str(alpha),format(base*factor,'.17g'),str(folder)],capture_output=True,text=True,timeout=240)
     (args.out/(folder.name+'.log')).write_text(result.stdout+result.stderr,encoding='utf-8');print(result.stdout.strip(),flush=True)
     if result.returncode:raise RuntimeError('execution failed '+str(folder))
    data=load(folder);material_check(data,alpha)
    if data['summary']['case']!=case or num(data['summary'],'alpha')!=alpha or num(data['summary'],'dt_s')!=base*factor or num(data['summary'],'duration_s')!=c['durations_s'][case]:raise ValueError('cached input mismatch')
    runs[(alpha,factor)]=data
  physical=runs[(1,c['dt_factors'][0])];physical_gate=compare(runs[(1,1)],physical,c,'integration');evaluations=[];allowed=set()
  for alpha in c['alphas']:
   fine=runs[(alpha,c['dt_factors'][0])];model_gate=compare(fine,physical,c,'surrogate')
   for factor in c['dt_factors']:
    run=runs[(alpha,factor)];integration=compare(run,fine,c,'integration');bulk=compare(run,physical,c,'surrogate')
    ok=physical_gate['accepted'] and model_gate['accepted'] and integration['accepted'] and bulk['accepted']
    evaluations.append({'alpha':alpha,'dt_factor':factor,'summary':run['summary'],'model_gate':model_gate,'integration':integration,'bulk':bulk,'accepted':ok})
    # Select only after establishing the best passing unsoftened candidate.
  best_unsoftened=max((e['dt_factor'] for e in evaluations if e['alpha']==1 and e['accepted']),default=0)
  allowed={(e['alpha'],e['dt_factor']) for e in evaluations if e['accepted'] and e['alpha']<1 and e['dt_factor']>best_unsoftened}
  common=allowed if common is None else common&allowed
  report['cases'][case]={'physical_reference_gate':physical_gate,'best_unsoftened_dt_factor':best_unsoftened,'runs':evaluations,'accelerated_scoped_candidates':[list(x) for x in sorted(allowed)]}
  (args.out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
 report['status']='completed';report['common_accelerated_candidates']=[list(x) for x in sorted(common or [])];report['decision']='no_common_candidate_defaults_preserved' if not common else 'experimental_common_candidates_only_no_runtime_enable'
 report['exe_sha256']=hashlib.sha256(args.exe.read_bytes()).hexdigest();sources=[*list((root/'Physics/PowderCore/include/powder').glob('*.hpp')),*list((root/'Physics/PowderCore/src').glob('*.cpp')),root/'Physics/PowderCore/app/softening_sweep.cpp',Path(__file__)]
 report['source_sha256']={f.relative_to(root).as_posix():hashlib.sha256(f.read_bytes()).hexdigest() for f in sources};(args.out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print('COMPLETED: '+report['decision'],flush=True)
if __name__=='__main__':main()
