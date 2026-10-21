"""Optional bounded target-mass smoke test, after the capacity matrix."""
import argparse,ctypes,hashlib,json,subprocess,time
from pathlib import Path
from run_optimization10_validation import Memory,ROOT
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True);ap.add_argument('--dense-only',action='store_true');ap.add_argument('--candidate-budget',type=int,default=32000000);a=ap.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
 contract={'count':1591550,'radius_m':.001,'density_kg_m3':1500,'threads':8,'measured_steps':5,'cold_steps':1,'dt_s':2e-8,'candidate_budget':a.candidate_budget,'memory_budget_bytes':8*1024**3,'timeout_s':180,'repeats':1,'scope':'resource-bounded smoke; no sustained process acceptance'}
 (out/'contract.json').write_text(json.dumps(contract,indent=2),encoding='utf-8')
 exe=ROOT/'Intermediate/PowderCore/Release/powder_capacity10.exe';api=ctypes.WinDLL('psapi');api.GetProcessMemoryInfo.argtypes=[ctypes.c_void_p,ctypes.POINTER(Memory),ctypes.c_ulong]
 results=[]
 for dense in ([1] if a.dense_only else [0,1]):
  print('TARGET',dense,flush=True);log=out/f'dense{dense}.json';err=out/f'dense{dense}.stderr'
  with log.open('w') as stdout,err.open('w') as stderr:
   p=subprocess.Popen([str(exe),'1591550','8',str(dense),'5',str(a.candidate_budget)],stdout=stdout,stderr=stderr);start=time.monotonic();peak=0;reason=None
   while p.poll() is None:
    m=Memory();m.cb=ctypes.sizeof(m)
    if api.GetProcessMemoryInfo(int(p._handle),ctypes.byref(m),ctypes.sizeof(m)):peak=max(peak,m.private)
    if peak>contract['memory_budget_bytes']:reason='private_memory_budget'
    if time.monotonic()-start>contract['timeout_s']:reason='timeout'
    if reason:p.kill();p.wait();break
    time.sleep(.05)
  result={'dense':bool(dense),'monitor_peak_private_bytes':peak,'process_wall_s':time.monotonic()-start,'status':reason or ('completed' if p.returncode==0 else 'rejected')}
  if p.returncode==0:result['measurement']=json.loads(log.read_text())
  else:result['error']=err.read_text()
  results.append(result);(out/'results.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
 report={'contract':contract,'results':results,'throughput_goal_achieved':False,'executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()}
 (out/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
if __name__=='__main__':main()
