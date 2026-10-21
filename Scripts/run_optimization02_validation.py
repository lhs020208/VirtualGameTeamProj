"""Optimization 02: same-input reference/cached correctness and timing evidence."""
import argparse,csv,datetime,hashlib,json,math,os,platform,statistics,subprocess,time,xml.etree.ElementTree as ET
from pathlib import Path
from run_optimization01_validation import sources,rows,CMAKE,memory_reader
ROOT=Path(__file__).resolve().parents[1]
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--output',type=Path,required=True);args=ap.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
 source=sources();source['Scripts/run_optimization02_validation.py']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
 report={'schema':1,'suite':'optimization02','passed':False,'large_scale_target_achieved':False,'source_files':source,'source_sha256':hashlib.sha256(json.dumps(source,sort_keys=True).encode()).hexdigest(),'started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'cpu':os.environ.get('PROCESSOR_IDENTIFIER'),'platform':platform.platform(),'logical_cpus':os.cpu_count(),'runs':[],'cases':[]}
 contract=json.loads((ROOT/'Physics/PowderCore/fixtures/optimization02_contract.json').read_text(encoding='utf-8'));(out/'contract.json').write_text(json.dumps(contract,indent=2)+'\n',encoding='utf-8')
 def run(cmd,name):
  print('RUN',name,flush=True);t=time.perf_counter();peak=0
  with (out/name).open('w',encoding='utf-8') as f:
   p=subprocess.Popen(list(map(str,cmd)),cwd=ROOT,stdout=f,stderr=subprocess.STDOUT,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0));sample,close=memory_reader(p.pid)
   try:
    while p.poll() is None:
     if sample:peak=max(peak,sample() or 0)
     if time.perf_counter()-t>600:p.kill();p.wait();raise TimeoutError(name)
     time.sleep(.025)
   finally:
    if close:close()
  report['runs'].append({'log':name,'command':list(map(str,cmd)),'exit_code':p.returncode,'wall_s':time.perf_counter()-t,'peak_working_set_bytes':peak})
  if p.returncode:raise RuntimeError(name+' failed')
 discrete={'case','status','requested_count','accepted_steps','pair_contacts','wall_contacts','arrived_count','mixing_valid','error','body_id','id','type','group','pebbles'}
 maxdiff=0.;comparisons=0
 def compare(a,b):
  nonlocal maxdiff,comparisons
  ar=rows(a);br=rows(b);assert len(ar)==len(br),(a,b,'rows')
  for x,y in zip(ar,br):
   assert x.keys()==y.keys()
   for k in x:
    if k in discrete:assert x[k]==y[k],(a.name,k,x[k],y[k]);continue
    av,bv=float(x[k]),float(y[k]);diff=abs(av-bv);tol=1e-12+1e-9*max(abs(av),abs(bv));assert math.isfinite(av) and math.isfinite(bv) and diff<=tol,(a.name,k,av,bv,tol)
    maxdiff=max(maxdiff,diff);comparisons+=1
 try:
  report['branch']=subprocess.check_output(['git','branch','--show-current'],cwd=ROOT,text=True).strip();assert report['branch']=='PowderSimulationOptimization'
  report['base_commit']=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
  build=ROOT/'Intermediate/PowderCore';run([CMAKE,'--build',build,'--config','Release','--parallel','8'],'build.log')
  run([CMAKE.with_name('ctest.exe'),'--test-dir',build,'-C','Release','--output-on-failure','--output-junit',out/'tests.xml'],'tests.log');tree=ET.parse(out/'tests.xml').getroot();assert int(tree.attrib['tests'])>=22 and int(tree.attrib.get('failures',0))==0
  run([CMAKE.with_name('ctest.exe'),'--test-dir',build,'-C','Release','-R','optimization_','--output-on-failure'],'focused_repeat.log')
  exe=build/'Release/powder_opt01.exe';report['executable_sha256']=hashlib.sha256(exe.read_bytes()).hexdigest();fixture=ROOT/'Physics/PowderCore/fixtures/optimization01.bench'
  for repeat in range(1,4):
   for mode in (('reference','cached') if repeat%2 else ('cached','reference')):
    name=f'{mode}_{repeat}';run([exe,fixture,out/name,0,mode],name+'.log')
  for mode in ('reference','cached'):run([exe,fixture,out/(mode+'_profile'),1,mode],mode+'_profile.log')
  reference=out/'reference_1';files=['physics.csv']+[p.name for p in reference.glob('*.csv') if p.name.endswith(('_initial.csv','_final.csv','_events.csv'))]
  for folder in [out/f'{mode}_{repeat}' for repeat in range(1,4) for mode in ('reference','cached')]+[out/(mode+'_profile') for mode in ('reference','cached')]:
   for name in files:compare(reference/name,folder/name)
  report['profile_and_repeat_exact']=all((out/f'{mode}_1'/name).read_bytes()==(folder/name).read_bytes() for mode in ('reference','cached') for folder in [out/f'{mode}_2',out/f'{mode}_3',out/(mode+'_profile')] for name in files)
  assert report['profile_and_repeat_exact']
  for base in rows(reference/'physics.csv'):
   case=base['case'];item={'name':case,'status':base['status']}
   if base['status']!='unsupported':
    for mode in ('reference','cached'):
     timings=[next(r for r in rows(out/f'{mode}_{repeat}/timings.csv') if r['case']==case) for repeat in range(1,4)]
     item[mode+'_median_loop_s']=statistics.median(float(r['loop_s']) for r in timings)
     item[mode+'_median_rate']=statistics.median(float(r['loop_rate']) for r in timings)
     item[mode+'_min_window_rate']=min(float(r['rate']) for repeat in range(1,4) for r in rows(out/f'{mode}_{repeat}/windows.csv') if r['case']==case)
     item[mode+'_phases_s']={r['phase']:float(r['seconds']) for r in rows(out/(mode+'_profile')/'profile.csv') if r['case']==case}
    item['speedup']=item['reference_median_loop_s']/item['cached_median_loop_s']
   report['cases'].append(item)
  report['numeric_comparisons']=comparisons;report['max_absolute_difference_across_units']=maxdiff
  after=sources();after['Scripts/run_optimization02_validation.py']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest();report['sources_unchanged']=source==after;assert report['sources_unchanged'];report['passed']=True
 except Exception as e:report['error']=str(e)
 (out/'report.json').write_text(json.dumps(report,indent=2,ensure_ascii=False,allow_nan=False)+'\n',encoding='utf-8')
 lines=['# Optimization 02 comparison','','Same inputs, Release CPU solver, 3 alternating unprofiled repeats per path. Timings include solver checks and fixture measurement. NullRHI/renderer performance is separate.','','| Case | Status | Reference rate | Cached rate | Speedup |','|---|---|---:|---:|---:|']
 for c in report['cases']:lines.append(f"| {c['name']} | {c['status']} | {c.get('reference_median_rate','n/a')} | {c.get('cached_median_rate','n/a')} | {c.get('speedup','n/a')} |")
 lines+=['','Large-scale target NOT achieved; unchanged 300-body cap. 30 RPM reference_rejected case is diagnostic only.',f"PASS: {report['passed']}",f"Source SHA256: {report['source_sha256']}"]
 (out/'report.md').write_text('\n'.join(lines)+'\n',encoding='utf-8');print('PASS' if report['passed'] else 'FAIL',report.get('error',''),flush=True);return 0 if report['passed'] else 1
if __name__=='__main__':raise SystemExit(main())
