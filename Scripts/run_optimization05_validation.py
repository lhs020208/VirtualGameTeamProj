"""Optimization 05 wall geometry/motion parity and phase timing evidence."""
import argparse,hashlib,json,math,statistics,subprocess
from pathlib import Path
from run_optimization01_validation import rows,sources
ROOT=Path(__file__).resolve().parents[1]
def manifest():
 result=sources();result['Scripts/run_optimization05_validation.py']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest();return result
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True);ap.add_argument('--reference-exe',type=Path,required=True);a=ap.parse_args();out=a.output.resolve();out.mkdir(exist_ok=True,parents=True)
 old=a.reference_exe.resolve();new=ROOT/'Intermediate/PowderCore/Release/powder_opt01.exe';fixture=ROOT/'Physics/PowderCore/fixtures/optimization01.bench'
 expected=json.loads((ROOT/'Docs/DEM/optimization04/report.json').read_text(encoding='utf-8'))['reuse_executable_sha256'];assert hashlib.sha256(old.read_bytes()).hexdigest()==expected
 before=manifest();report={'passed':False,'large_scale_target_achieved':False,'reference_executable_sha256':expected,'walls_executable_sha256':hashlib.sha256(new.read_bytes()).hexdigest(),'source_files':before,'comparison':'same-input 3 alternating repeats; abs 1e-12 + rel 1e-9; discrete counts exact; source unchanged','cases':[]}
 def run(name,exe,mode,profile):
  print('RUN',name,flush=True)
  with (out/(name+'.log')).open('w',encoding='utf-8') as f:subprocess.run([str(exe),str(fixture),str(out/name),str(profile),mode],cwd=ROOT,stdout=f,stderr=subprocess.STDOUT,check=True)
 for repeat in range(1,4):
  for mode in (('old','walls') if repeat%2 else ('walls','old')):run(f'{mode}_{repeat}',old if mode=='old' else new,'reuse' if mode=='old' else 'walls',0)
 for mode in ('old','walls'):run(mode+'_profile',old if mode=='old' else new,'reuse' if mode=='old' else 'walls',1)
 run('legacy_current',new,'reuse',1)
 base=out/'old_1';files=['physics.csv']+[p.name for p in base.glob('*.csv') if p.name.endswith(('_initial.csv','_final.csv','_events.csv'))]
 discrete={'case','status','requested_count','accepted_steps','pair_contacts','wall_contacts','arrived_count','mixing_valid','error','body_id','id','type','group','pebbles'};maxdiff=0.;comparisons=0
 for folder in [out/f'{m}_{i}' for m in ('old','walls') for i in range(1,4)]+[out/(m+'_profile') for m in ('old','walls')]+[out/'legacy_current']:
  for name in files:
   ar,br=rows(base/name),rows(folder/name);assert len(ar)==len(br),(folder,name,'row count')
   for x,y in zip(ar,br):
    assert x.keys()==y.keys()
    for k in x:
     if k in discrete:assert x[k]==y[k],(folder,name,k,x[k],y[k]);continue
     av,bv=float(x[k]),float(y[k]);diff=abs(av-bv);tol=1e-12+1e-9*max(abs(av),abs(bv));assert math.isfinite(av) and math.isfinite(bv) and diff<=tol,(folder,name,k,av,bv,tol);maxdiff=max(maxdiff,diff);comparisons+=1
 report['legacy_physics_exact']=all((base/n).read_bytes()==(out/'legacy_current'/n).read_bytes() for n in files);assert report['legacy_physics_exact']
 report['repeat_and_profiling_exact']=all((out/f'{m}_1'/n).read_bytes()==(f/n).read_bytes() for m in ('old','walls') for f in [out/f'{m}_2',out/f'{m}_3',out/(m+'_profile')] for n in files);assert report['repeat_and_profiling_exact']
 for row in rows(base/'physics.csv'):
  case=row['case'];item={'case':case,'status':row['status']}
  if row['status']!='unsupported':
   for mode in ('old','walls'):
    ts=[next(r for r in rows(out/f'{mode}_{i}/timings.csv') if r['case']==case) for i in range(1,4)];item[mode+'_rate']=statistics.median(float(r['loop_rate']) for r in ts);item[mode+'_loop_s']=statistics.median(float(r['loop_s']) for r in ts)
    item[mode+'_min_window_rate']=min(float(r['rate']) for i in range(1,4) for r in rows(out/f'{mode}_{i}/windows.csv') if r['case']==case)
    phases=[r for r in rows(out/(mode+'_profile')/'profile.csv') if r['case']==case];item[mode+'_phases_s']={r['phase']:float(r['seconds']) for r in phases}
    item[mode+'_wall_phases_s']=sum(float(r['seconds']) for r in phases if r['phase'] in ('static_mesh_query_and_force','moving_mesh_query_and_force'))
    counts=phases[0] if mode=='walls' else next(r for r in rows(out/'legacy_current/profile.csv') if r['case']==case)
    for key in ('wall_pose_builds','wall_triangle_checks','wall_analytic_queries','wall_bounds_rejects'):item[mode+'_'+key]=int(counts.get(key,0))
   item['speedup']=item['old_loop_s']/item['walls_loop_s'];item['wall_phase_speedup']=item['old_wall_phases_s']/item['walls_wall_phases_s']
  report['cases'].append(item)
 assert before==manifest();report['numeric_comparisons']=comparisons;report['max_absolute_difference_across_units']=maxdiff;report['passed']=True
 (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report['cases'],indent=2),flush=True)
if __name__=='__main__':main()
