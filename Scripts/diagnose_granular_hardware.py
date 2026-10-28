"""Measure the existing binary 50k fixture without changing contact physics.
GPU metrics cover the whole adapter; event times cover only this OpenCL queue.
"""
from pathlib import Path
import argparse,subprocess,time,json,hashlib,statistics
import psutil
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('output',type=Path);p.add_argument('--steps',type=int,default=10688);p.add_argument('--case',default='all');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 initial=ROOT/'Saved/Rate01Recheck/current12/initial.bin'
 expected='9547042760b376c83d3820068abe4e52b4093235062f4476a97c3fbb1cb8e29d'
 if hashlib.sha256(initial.read_bytes()).hexdigest()!=expected:raise ValueError('current fixture SHA mismatch')
 cases={'base':(32,128,0,50,0),'profile':(32,128,1,50,0),'split':(32,128,2,50,0),'batch128':(128,128,0,50,0),'local64':(32,64,0,50,0),'local256':(32,256,0,50,0),'no_reads':(32,128,0,0,0),'rate01':(32,128,0,50,.1),'base_repeat':(32,128,0,50,0)}
 chosen=cases if a.case=='all' else {k:cases[k] for k in a.case.split(',')}
 for name,args in chosen.items():
  dest=a.output/name
  if dest.exists():raise ValueError('will not overwrite '+str(dest))
  cmd=[str(ROOT/'Intermediate/PowderCore/Release/granular_diagnose.exe'),str(initial),str(dest),str(a.steps),*map(str,args)]
  with (a.output/(name+'.log')).open('w') as log:
   proc=subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW);child=psutil.Process(proc.pid);samples=[];t0=time.perf_counter()
   while proc.poll() is None:
    if time.perf_counter()-t0>600:
     proc.terminate();proc.wait(timeout=30);raise RuntimeError('own diagnostic timeout')
    try:
     cpu=child.cpu_times();threads=child.threads()
     gpu=subprocess.run(['nvidia-smi','--query-gpu=utilization.gpu,utilization.memory,power.draw,memory.used,clocks.sm,temperature.gpu','--format=csv,noheader,nounits'],capture_output=True,text=True,creationflags=subprocess.CREATE_NO_WINDOW,timeout=5)
     values=[float(v.strip()) for v in gpu.stdout.strip().split(',')] if gpu.returncode==0 and gpu.stdout.strip() else []
     if len(values)!=6 or values[5]>=75 or values[2]>=155:
      proc.terminate();proc.wait(timeout=30);raise RuntimeError('Conservative hardware stop: telemetry unavailable, temperature >=75C, or power >=155W')
     samples.append({'elapsed_s':time.perf_counter()-t0,'process_cpu_s':cpu.user+cpu.system,'threads':{str(t.id):t.user_time+t.system_time for t in threads},'gpu':gpu.stdout.strip(),'editor_cpu_s':sum(sum(q.cpu_times()[:2]) for q in psutil.process_iter(['name']) if q.info['name']=='UnrealEditor.exe')})
    except psutil.NoSuchProcess:break
    except Exception:
     if proc.poll() is None:proc.terminate();proc.wait(timeout=30)
     raise
    time.sleep(.5)
   code=proc.wait()
  (a.output/(name+'_monitor.json')).write_text(json.dumps(samples,indent=2))
  if code:raise RuntimeError(name+' failed: '+(a.output/(name+'.log')).read_text())
  r=json.loads((dest/'result.json').read_text());print(name,json.dumps(r),flush=True)
 (a.output/'fixture.json').write_text(json.dumps({'initial_sha256':expected,'steps':a.steps,'dt':.000375,'contact_iterations':16,'rpm':30,'rotation_start_step':2672,'scope':'same saved initial positions and virtual materials; fixed control boundary, not a frame-exact UE replay; editor remains open'},indent=2))
if __name__=='__main__':main()
