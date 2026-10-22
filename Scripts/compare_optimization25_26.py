import csv,json,math,statistics,argparse
from pathlib import Path

def compare(root):
 def rows(name):
  with (root/name/'windows.csv').open(encoding='utf-8') as f:return list(csv.DictReader(f))
 a=rows('baseline24');b=rows('final26');assert len(a)==len(b)==33
 max_difference={k:0. for k in ['mass_a_kg','mass_b_kg','overlap_m','torque_z_nm','wall_work_j','mix_index']}
 for old,new in zip(a,b):
  for key in ['repeat','window','bodies','dt_s','time_s','batch_steps','momentum_checked','contacts','wall_contacts']:assert old[key]==new[key],key
  for key in max_difference:
   delta=abs(float(old[key])-float(new[key]));assert delta<1e-10,key;max_difference[key]=max(max_difference[key],delta)
  for key,value in new.items():
   if value:assert math.isfinite(float(value)),key
  assert float(new['compute_s'])>0 and float(new['measurement_snapshot_s'])>0
  assert math.isclose(float(new['inclusive_rate']),int(new['batch_steps'])*float(new['dt_s'])/(float(new['compute_s'])+float(new['measurement_snapshot_s'])),rel_tol=1e-12)
  if new['momentum_checked']=='1':assert float(new['momentum_residual'])<1e-10
 for name in ['baseline24','final26']:
  report=json.loads((root/name/'acceptance.json').read_text());assert report['accuracy_gate'] and report['completed_windows']==33 and not report['accepted']
 def med(data,key,extended=False):return statistics.median(float(r[key]) for r in data if (int(r['window'])>=6 if extended else 0<int(r['window'])<6))
 summary={'numerical_comparison_passed':True,'maximum_differences':max_difference,'baseline_warm_rate':med(a,'inclusive_rate'),'final_warm_rate':med(b,'inclusive_rate'),'baseline_batch_rate':med(a,'inclusive_rate',True),'final_batch_rate':med(b,'inclusive_rate',True),'warm_speedup':med(b,'inclusive_rate')/med(a,'inclusive_rate'),'batch_speedup':med(b,'inclusive_rate',True)/med(a,'inclusive_rate',True),'baseline_sort_gpu_s':med(a,'sort_gpu_s'),'final_sort_gpu_s':med(b,'sort_gpu_s'),'required_rate':.1,'target_accepted':False,'render_included':False,'physical_time_per_repeat_s':6.52e-6}
 with (root/'sort_benchmark.csv').open() as f:micro=list(csv.DictReader(f))
 summary['microbench']=[{'count':n,'forced_rebuild':force,'variant':v,'median_seconds':statistics.median(float(r['seconds']) for r in micro if int(r['count'])==n and int(r['force_rebuild'])==force and int(r['variant'])==v)} for n in [129,32769] for force in [0,1] for v in [0,1,2]]
 (root/'comparison.json').write_text(json.dumps(summary,indent=2),encoding='utf-8');print(json.dumps(summary,indent=2))
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('results',type=Path);compare(p.parse_args().results)
