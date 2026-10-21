"""Optimization 01 baseline/profiling evidence. Physics and algorithms remain unchanged."""
import argparse,ctypes,datetime,hashlib,json,os,platform,shutil,statistics,subprocess,sys,time,xml.etree.ElementTree as ET
from pathlib import Path
import csv
ROOT=Path(__file__).resolve().parents[1]
CMAKE=Path(r'C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe')
def sources():
 paths=[]
 for folder in ('Physics/PowderCore','Source'):
  paths.extend(p for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix in ('.cpp','.h','.hpp','.cs','.txt','.py','.bench','.json') and '__pycache__' not in p.parts)
 paths.append(Path(__file__))
 return {p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths)}
def rows(path):
 with path.open(encoding='utf-8-sig',newline='') as f:return list(csv.DictReader(f))
def memory_reader(pid):
 if os.name!='nt':return None,None
 class Counters(ctypes.Structure):
  _fields_=[('cb',ctypes.c_ulong),('faults',ctypes.c_ulong)]+[(k,ctypes.c_size_t) for k in ('peak_working','working','peak_pool_paged','pool_paged','peak_pool_nonpaged','pool_nonpaged','pagefile','peak_pagefile')]
 kernel=ctypes.WinDLL('kernel32',use_last_error=True);psapi=ctypes.WinDLL('psapi',use_last_error=True)
 kernel.OpenProcess.argtypes=[ctypes.c_ulong,ctypes.c_int,ctypes.c_ulong];kernel.OpenProcess.restype=ctypes.c_void_p;kernel.CloseHandle.argtypes=[ctypes.c_void_p]
 psapi.GetProcessMemoryInfo.argtypes=[ctypes.c_void_p,ctypes.POINTER(Counters),ctypes.c_ulong]
 handle=kernel.OpenProcess(0x410,0,pid)
 if not handle:return None,None
 def sample():
  c=Counters();c.cb=ctypes.sizeof(c)
  return int(c.peak_working) if psapi.GetProcessMemoryInfo(handle,ctypes.byref(c),c.cb) else None
 return sample,lambda:kernel.CloseHandle(handle)
def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
 before=sources();start=time.perf_counter();report={'schema':1,'suite':'optimization01_v1','started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'passed':False,'large_scale_target_achieved':False,'source_files':before,'source_sha256':hashlib.sha256(json.dumps(before,sort_keys=True).encode()).hexdigest(),'platform':platform.platform(),'cpu':os.environ.get('PROCESSOR_IDENTIFIER',platform.processor()),'logical_cpus':os.cpu_count(),'configuration':'Release','runs':[]}
 contract=ROOT/'Physics/PowderCore/fixtures/optimization01_contract.json';shutil.copyfile(contract,out/contract.name)
 def run(cmd,name,timeout=600):
  print('RUN',name,flush=True);t=time.perf_counter();peak=None
  with (out/name).open('w',encoding='utf-8') as f:
   p=subprocess.Popen(list(map(str,cmd)),cwd=ROOT,stdout=f,stderr=subprocess.STDOUT,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0));sample,close=memory_reader(p.pid)
   try:
    while p.poll() is None:
     if sample:
      value=sample()
      if value is not None:peak=max(peak or 0,value)
     if time.perf_counter()-t>timeout:p.kill();p.wait();raise TimeoutError(name)
     time.sleep(.025)
    if sample:
     value=sample()
     if value is not None:peak=max(peak or 0,value)
   finally:
    if close:close()
  report['runs'].append({'log':name,'command':list(map(str,cmd)),'exit_code':p.returncode,'wall_s':time.perf_counter()-t,'child_peak_working_set_bytes':peak})
  if p.returncode:raise RuntimeError(name+' failed')
 try:
  git=subprocess.run(['git','rev-parse','HEAD'],cwd=ROOT,capture_output=True,text=True,check=True);report['base_commit']=git.stdout.strip()
  report['branch']=subprocess.run(['git','branch','--show-current'],cwd=ROOT,capture_output=True,text=True,check=True).stdout.strip()
  try:report['gpu']=subprocess.run(['nvidia-smi','--query-gpu=name,memory.total,driver_version','--format=csv,noheader'],capture_output=True,text=True,timeout=10,check=True).stdout.strip()
  except Exception:report['gpu']=None
  build=ROOT/'Intermediate/PowderCore';run([CMAKE,'-S',ROOT/'Physics/PowderCore','-B',build],'configure.log');run([CMAKE,'--build',build,'--config','Release'],'build.log')
  report['executable_sha256']=hashlib.sha256((build/'Release/powder_opt01.exe').read_bytes()).hexdigest()
  for repeat in (1,2):
   xml=out/f'tests_{repeat}.xml';run([CMAKE.with_name('ctest.exe'),'--test-dir',build,'-C','Release','--output-on-failure','--output-junit',xml],f'tests_{repeat}.log')
   tree=ET.parse(xml).getroot();assert int(tree.attrib['tests'])>=21 and int(tree.attrib.get('failures',0))==0
   for mode in (0,1):
    target=out/f'run_{repeat}_profile_{mode}';run([build/'Release/powder_opt01.exe',ROOT/'Physics/PowderCore/fixtures/optimization01.bench',target,mode],f'run_{repeat}_profile_{mode}.log')
    results=rows(target/'physics.csv');assert len(results)==14 and sum(r['status']=='passed' for r in results)==9 and sum(r['status']=='reference_rejected' for r in results)==1 and sum(r['status']=='unsupported' for r in results)==4
  reference=out/'run_1_profile_0';files=['physics.csv']+[p.name for p in reference.glob('*_*.csv') if p.name.endswith(('_initial.csv','_final.csv','_events.csv'))]
  report['physics_exact_repeat_and_profile_parity']=all((reference/name).read_bytes()==(out/f'run_{repeat}_profile_{mode}'/name).read_bytes() for repeat in (1,2) for mode in (0,1) for name in files)
  report['cases']=[]
  for base in rows(reference/'physics.csv'):
   case=base['case'];item={'physics':base}
   if base['status']!='unsupported':
    time_rows=[next(r for r in rows(out/f'run_{repeat}_profile_{mode}/timings.csv') if r['case']==case) for repeat in (1,2) for mode in (0,1)]
    off=[r for r in time_rows if r['profile_enabled']=='0'];on=[r for r in time_rows if r['profile_enabled']=='1']
    item['unprofiled_loop_rate_median']=statistics.median(float(r['loop_rate']) for r in off)
    item['profiled_to_unprofiled_solver_time_ratio']=statistics.median(float(r['solver_call_s']) for r in on)/statistics.median(float(r['solver_call_s']) for r in off)
    window_rows=[r for repeat in (1,2) for r in rows(out/f'run_{repeat}_profile_0/windows.csv') if r['case']==case]
    item['minimum_short_window_rate']=min(float(r['rate']) for r in window_rows)
    phase_rows=[r for repeat in (1,2) for r in rows(out/f'run_{repeat}_profile_1/profile.csv') if r['case']==case];phase_times={name:statistics.median(float(r['seconds']) for r in phase_rows if r['phase']==name) for name in {r['phase'] for r in phase_rows}}
    total=sum(phase_times.values());item['phase_percent']={k:100*v/total for k,v in sorted(phase_times.items(),key=lambda x:-x[1])};item['counters']={k:int(phase_rows[0][k]) for k in ('attempted_steps','accepted_steps','pair_distance_checks','post_pair_checks','material_pair_validations')}
   report['cases'].append(item)
  report['sources_unchanged']=before==sources();report['passed']=report['sources_unchanged'] and report['physics_exact_repeat_and_profile_parity']
 except Exception as e:report['error']=str(e)
 report['wall_s']=time.perf_counter()-start;(out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2,allow_nan=False)+'\n',encoding='utf-8')
 lines=['# Optimization 01 baseline','',f"Status: {'PASS' if report['passed'] else 'FAIL'}",'','This validates instrumentation/reference fixtures, NOT sustained 0.1x or large-scale support. 30 RPM mixing is retained as a reference-rejected diagnostic; do not use it as a validated accurate baseline.','', '| Case | Status | Unprofiled loop rate | Minimum short window rate | Largest measured phase |','|---|---|---:|---:|---|']
 for item in report.get('cases',[]):
  p=item['physics'];phase=next(iter(item.get('phase_percent',{})),'n/a');lines.append(f"| {p['case']} | {p['status']} | {item.get('unprofiled_loop_rate_median','n/a')} | {item.get('minimum_short_window_rate','n/a')} | {phase} |")
 lines+=['','All large requests are unsupported (300-body limit unchanged). Phase clocks are exclusive broad groups; query/force internals are not separately timed. CPU display synchronization and GPU drawing are outside this standalone runner. Source hash: '+report['source_sha256']]
 (out/'report.md').write_text('\n'.join(lines)+'\n',encoding='utf-8');print('PASS' if report['passed'] else 'FAIL',out);return 0 if report['passed'] else 1
if __name__=='__main__':sys.exit(main())

