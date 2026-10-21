"""Bounded, reproducible CPU capacity acceptance. No surrogate or GPU solver."""
import argparse,ctypes,hashlib,json,math,platform,statistics,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class Memory(ctypes.Structure):
 _fields_=[('cb',ctypes.c_ulong),('faults',ctypes.c_ulong)]+[(n,ctypes.c_size_t) for n in ['peak_ws','ws','peak_paged','paged','peak_nonpaged','nonpaged','pagefile','peak_pagefile','private']]
def axial_contact_bound(n):
 side=math.ceil(n**(1/3))
 return sum((i%side+1<side and i+1<n)+((i//side)%side+1<side and i+side<n)+(i+side*side<n) for i in range(n))
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True);args=ap.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
 exe=ROOT/'Intermediate/PowderCore/Release/powder_capacity10.exe'
 contract={'counts':[1000,10000,100000],'threads':[1,8],'repeats':3,'steps':{'1000':200,'10000':40,'100000':10},'radius_m':.001,'density_kg_m3':1500,'mass_per_group_target_kg':5,'dt_s':2e-8,'kn':10000,'cn':.01,'kt':2500,'ct':.0025,'mu':.4,'fixtures':['noncontact_freefall','dense_contact_lattice'],'private_memory_limit_bytes':4*1024**3,'child_timeout_s':180,'maximum_candidates':4000000,'target_rate':.1,'accuracy':{'freefall_absolute_m':1e-12,'momentum_absolute_kg_m_s':1e-12,'momentum_relative':1e-9,'mass_absolute_kg':1e-10,'mass_relative':1e-10,'maximum_overlap_radius_ratio':.01,'serial_parallel':'same-state FNV64 fingerprint and physical metrics exact; 128-body full-state exhaustive/parallel oracle in CTest'},'scope':'short fixed-step CPU throughput; no sustained mixing/discharge/full-process or real-material validation'}
 contract['target_count']=2*math.ceil(5/(4/3*math.pi*.001**3*1500))
 (out/'contract.json').write_text(json.dumps(contract,indent=2),encoding='utf-8')
 subprocess.run([str(exe)],stdout=(out/'capacity_tests.log').open('w'),stderr=subprocess.STDOUT,check=True)
 psapi=ctypes.WinDLL('psapi');psapi.GetProcessMemoryInfo.argtypes=[ctypes.c_void_p,ctypes.POINTER(Memory),ctypes.c_ulong];psapi.GetProcessMemoryInfo.restype=ctypes.c_int
 samples=[]
 for repeat in range(3):
  for count in contract['counts']:
   for dense in [False,True]:
    for threads in ([1,8] if repeat%2==0 else [8,1]):
     name=f'n{count}_dense{int(dense)}_t{threads}_r{repeat}';print('RUN',name,flush=True);log=out/(name+'.json');err=out/(name+'.stderr')
     with log.open('w') as stdout,err.open('w') as stderr:
      p=subprocess.Popen([str(exe),str(count),str(threads),str(int(dense)),str(contract['steps'][str(count)])],stdout=stdout,stderr=stderr)
      start=time.monotonic();peak=0;reason=None
      while p.poll() is None:
       m=Memory();m.cb=ctypes.sizeof(m)
       if psapi.GetProcessMemoryInfo(int(p._handle),ctypes.byref(m),ctypes.sizeof(m)):peak=max(peak,m.private)
       if peak>contract['private_memory_limit_bytes']:reason='private_memory_budget'
       if time.monotonic()-start>contract['child_timeout_s']:reason='timeout'
       if reason:p.kill();p.wait();break
       time.sleep(.05)
     if p.returncode!=0:raise RuntimeError(f'{name}: {reason or err.read_text()}')
     row=json.loads(log.read_text());row.update(repeat=repeat,monitor_peak_private_bytes=peak);samples.append(row)
     (out/'samples.json').write_text(json.dumps(samples,indent=2),encoding='utf-8')
 summaries=[]
 for count in contract['counts']:
  for dense in [False,True]:
   rows=[r for r in samples if r['count']==count and r['dense']==dense];reference=rows[0]
   for row in rows:
    for key in ['state_fnv64','mass_kg','mass_a_kg','mass_b_kg','physical_s','history_count','kinetic_j','max_pair_contacts','max_overlap_radius_ratio','freefall_error_m','momentum_error']:assert row[key]==reference[key],(count,dense,key)
   serial=statistics.median(r['loop_s'] for r in rows if r['threads']==1)
   for threads in [1,8]:
    group=[r for r in rows if r['threads']==threads];med=statistics.median(r['loop_s'] for r in group)
    summaries.append({'count':count,'dense':dense,'threads':threads,'median_rate':statistics.median(r['rate'] for r in group),'minimum_step_rate':min(r['minimum_step_rate'] for r in group),'median_cold_inclusive_rate':statistics.median(r['cold_inclusive_rate'] for r in group),'speedup':serial/med,'mass_kg':reference['mass_kg'],'max_peak_working_set_bytes':max(r['peak_working_set_bytes'] for r in group),'max_private_bytes':max(max(r['private_bytes'],r['monitor_peak_private_bytes']) for r in group),'phase_s_median':[statistics.median(r['phase_s'][i] for r in group) for i in range(9)],'target_rate_met':False})
 assert all(s['median_rate']<contract['target_rate'] for s in summaries)
 peak=max(s['max_private_bytes'] for s in summaries if s['count']==100000)
 estimated=math.ceil(peak*contract['target_count']/100000*1.5)
 target={'count':contract['target_count'],'status':'not_executed','reason':'capacity/resource preflight; dense candidate count exceeds current 4M budget; short-step CPU rate already fails at 100k; full-process untested','estimated_private_bytes_with_50pct_headroom':estimated,'estimate_not_measurement':True,'maximum_candidate_budget':4000000,'dense_lattice_axial_contacts_lower_bound':axial_contact_bound(contract['target_count']),'physical_target_achieved':False}
 # Additional common-time integration check; separate from throughput timings.
 convergence=[]
 for count in contract['counts']:
  value=json.loads(subprocess.check_output([str(ROOT/'Intermediate/PowderCore/Release/capacity_convergence.exe'),str(count)],text=True))
  convergence.append(value)
  (out/f'convergence_{count}.json').write_text(json.dumps(value,indent=2),encoding='utf-8')
 # Preserve the exact existing default path, including diagnostic failures.
 for name,program in [('before',ROOT/'Saved/Optimization10_before/powder_opt01.exe'),('default',ROOT/'Intermediate/PowderCore/Release/powder_opt01.exe')]:
  p=subprocess.run([str(program),str(ROOT/'Physics/PowderCore/fixtures/optimization01.bench'),str(out/name),'1','walls'],capture_output=True,text=True,check=True)
  (out/(name+'.log')).write_text(p.stdout,encoding='utf-8')
 files=[p.name for p in (out/'before').glob('*.csv') if p.name=='physics.csv' or p.name.endswith(('_initial.csv','_final.csv','_events.csv'))]
 for name in files:assert (out/'before'/name).read_bytes()==(out/'default'/name).read_bytes(),name
 sources={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for folder in ['Physics/PowderCore/include','Physics/PowderCore/src','Physics/PowderCore/app','Physics/PowderCore/tests'] for p in (ROOT/folder).rglob('*') if p.suffix in ['.cpp','.hpp','.py']}
 report={'acceptance_work_complete':True,'throughput_goal_achieved':False,'contract':contract,'summaries':summaries,'target_total':target,'default_exact_csv_files':len(files),'large_state_comparisons':len(samples),'convergence':convergence,'source_sha256':sources,'executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'reference_sha256':hashlib.sha256((ROOT/'Saved/Optimization10_before/powder_opt01.exe').read_bytes()).hexdigest(),'machine':platform.platform(),'decision':'Proceed to 11 representative-particle contract; preserve ratio=1 CPU oracle. Plan 14 GPU feasibility after 13 if reduction passes; no claim that either alone achieves 0.1x.'}
 (out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(summaries,indent=2),flush=True)
if __name__=='__main__':main()
