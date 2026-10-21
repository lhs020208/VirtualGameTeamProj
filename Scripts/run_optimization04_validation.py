"""Optimization 04: compare preserved optimization-03 binary with neighbor/workspace reuse."""
import argparse,hashlib,json,statistics,subprocess
from pathlib import Path
from run_optimization01_validation import rows,sources
ROOT=Path(__file__).resolve().parents[1]
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True);ap.add_argument('--reference-exe',type=Path,required=True);args=ap.parse_args();out=args.output.resolve();out.mkdir(exist_ok=True,parents=True)
 old=args.reference_exe.resolve();new=ROOT/'Intermediate/PowderCore/Release/powder_opt01.exe';fixture=ROOT/'Physics/PowderCore/fixtures/optimization01.bench'
 expected=json.loads((ROOT/'Docs/DEM/optimization03/report.json').read_text(encoding='utf-8'))['grid_executable_sha256'];assert hashlib.sha256(old.read_bytes()).hexdigest()==expected,'not the recorded optimization03 executable'
 before=sources();before['Scripts/run_optimization04_validation.py']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest();report={'passed':False,'large_scale_target_achieved':False,'reference_executable_sha256':expected,'reuse_executable_sha256':hashlib.sha256(new.read_bytes()).hexdigest(),'source_files':before,'cases':[],'comparison':'exact physical CSV bytes; same inputs; 3 alternating repeats; profile on/off; archived task03 binary vs task04 reuse; exact results; retained capacities exclude other solver/mesh allocations'}
 for repeat in range(1,4):
  for mode in (('old','grid') if repeat%2 else ('grid','old')):
   name=f'{mode}_{repeat}';print('RUN',name,flush=True)
   with (out/(name+'.log')).open('w',encoding='utf-8') as f:subprocess.run([str(old if mode=='old' else new),str(fixture),str(out/name),'0','grid' if mode=='old' else 'reuse'],cwd=ROOT,stdout=f,stderr=subprocess.STDOUT,check=True)
 for mode in ('old','grid'):
  name=mode+'_profile';print('RUN',name,flush=True)
  with (out/(name+'.log')).open('w',encoding='utf-8') as f:subprocess.run([str(old if mode=='old' else new),str(fixture),str(out/name),'1','grid' if mode=='old' else 'reuse'],cwd=ROOT,stdout=f,stderr=subprocess.STDOUT,check=True)
 with (out/'forced_rebuild.log').open('w',encoding='utf-8') as f:subprocess.run([str(new),str(fixture),str(out/'forced_rebuild'),'1','grid'],cwd=ROOT,stdout=f,stderr=subprocess.STDOUT,check=True)
 base=out/'old_1';files=['physics.csv']+[p.name for p in base.glob('*.csv') if p.name.endswith(('_initial.csv','_final.csv','_events.csv'))]
 for folder in [out/f'{m}_{i}' for m in ('old','grid') for i in range(1,4)]+[out/(m+'_profile') for m in ('old','grid')]+[out/'forced_rebuild']:
  for name in files:assert (base/name).read_bytes()==(folder/name).read_bytes(),(folder,name,'physics mismatch')
 for row in rows(base/'physics.csv'):
  c=row['case'];item={'case':c,'status':row['status']}
  if row['status']!='unsupported':
   for mode in ('old','grid'):
    ts=[next(r for r in rows(out/f'{mode}_{i}/timings.csv') if r['case']==c) for i in range(1,4)]
    item[mode+'_rate']=statistics.median(float(r['loop_rate']) for r in ts)
    item[mode+'_loop_s']=statistics.median(float(r['loop_s']) for r in ts)
    item[mode+'_min_window_rate']=min(float(r['rate']) for i in range(1,4) for r in rows(out/f'{mode}_{i}/windows.csv') if r['case']==c)
    pr=next(r for r in rows(out/(mode+'_profile')/'profile.csv') if r['case']==c)
    item[mode+'_neighbor_rebuilds']=int(pr.get('neighbor_rebuilds',0));item[mode+'_neighbor_reuses']=int(pr.get('neighbor_reuses',0));item[mode+'_neighbor_capacity_bytes']=int(pr.get('neighbor_capacity_bytes',0));item[mode+'_workspace_capacity_bytes']=int(pr.get('workspace_capacity_bytes',0));item[mode+'_pair_checks']=int(pr['pair_distance_checks']);item[mode+'_post_checks']=int(pr['post_pair_checks'])
   item['speedup']=item['old_loop_s']/item['grid_loop_s']
  report['cases'].append(item)
 after=sources();after['Scripts/run_optimization04_validation.py']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest();assert before==after,'source changed during benchmark';report['passed']=True
 (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report['cases'],indent=2),flush=True)
if __name__=='__main__':main()
