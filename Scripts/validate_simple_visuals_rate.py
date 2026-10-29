"""30 FPS rate validation of the saved simplified-material binary scene.
Uses process-local console overrides; never saves map/rate changes.
"""
from pathlib import Path
import argparse,subprocess,time,json,hashlib,csv,statistics,shutil,os
import psutil
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('output',type=Path);p.add_argument('--seconds',type=int,default=30);p.add_argument('--rate',type=float,default=2);p.add_argument('--paused',action='store_true');p.add_argument('--coupled',action='store_true');p.add_argument('--box',action='store_true');p.add_argument('--cohesion',type=float,default=0);p.add_argument('--wall-adhesion',type=float,default=0);p.add_argument('--map',default='/Game/DEM/Tests/L_MX301_BinaryMaterials');a=p.parse_args()
 assert 0<a.rate<=2 and 1<=a.seconds<=120
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
 profile=Path(os.environ['LOCALAPPDATA'])/'UnrealEngine/5.8/Saved/Profiling/CSV';old=set(profile.glob('*'))
 cmd=['C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe',str(ROOT/'VirtualGameTeamProj.uproject'),a.map,'-game','-windowed','-ResX=1280','-ResY=720','-unattended','-nosound','-nop4','-ddc=InstalledNoZenLocalFallback','-GrainBenchmark',f'-GrainSeconds={a.seconds}','-GrainAuditSnapshots',f'-GrainOutput={out}',f'-abslog={out}/engine.log',f'-ExecCmds=t.MaxFPS 30,set PowderGranularMixer TargetRate {a.rate},getall PowderGranularMixer TargetRate',f'-csvCaptureFrames={max(600,int(a.seconds/a.rate*30*.85))}','-csvCompression=0']
 if a.coupled:cmd.append('-GrainCoupledGridAir')
 if a.box:cmd.append('-GrainCoupledBox')
 if a.cohesion or a.wall_adhesion:cmd.extend([f'-GrainCohesionGamma={a.cohesion}',f'-GrainWallAdhesionGamma={a.wall_adhesion}'])
 if a.paused:
  i=next(i for i,v in enumerate(cmd) if v.startswith('-ExecCmds='));cmd[i]+=',set PowderGranularMixer Running false'
 (out/'command.json').write_text(json.dumps(cmd,indent=2));samples=[];start=time.perf_counter();timeout=max(180,a.seconds/min(a.rate,.15)*1.8)
 with (out/'console.log').open('w') as log:
  proc=subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW);child=psutil.Process(proc.pid)
  try:
   while proc.poll() is None:
    if time.perf_counter()-start>timeout:raise RuntimeError('own benchmark timeout')
    cpu=child.cpu_times();gpu=subprocess.run(['nvidia-smi','--query-gpu=utilization.gpu,utilization.memory,power.draw,memory.used,clocks.sm,temperature.gpu','--format=csv,noheader,nounits'],capture_output=True,text=True,creationflags=subprocess.CREATE_NO_WINDOW,timeout=5)
    v=[float(x.strip()) for x in gpu.stdout.strip().split(',')] if gpu.returncode==0 else []
    if len(v)!=6 or v[5]>=75 or v[2]>=155:raise RuntimeError('Conservative safety stop: missing telemetry, >=75C or >=155W')
    samples.append({'elapsed_s':time.perf_counter()-start,'process_cpu_s':cpu.user+cpu.system,'gpu':gpu.stdout.strip()})
    time.sleep(.5)
  except psutil.NoSuchProcess:pass
  except BaseException:
   if proc.poll() is None:proc.terminate();proc.wait(timeout=30)
   raise
  finally:(out/'monitor.json').write_text(json.dumps(samples,indent=2))
  code=proc.wait()
 if code or not (out/'result.json').exists():raise RuntimeError(f'benchmark failed {code}')
 assert hashlib.sha256((out/'initial.bin').read_bytes()).hexdigest()=='9547042760b376c83d3820068abe4e52b4093235062f4476a97c3fbb1cb8e29d'
 result=json.loads((out/'result.json').read_text(encoding='utf-8-sig'));assert not result['error'] and result['time']>=a.seconds
 rows=list(csv.DictReader((out/'trace.csv').open()));windows=[]
 for i,first in enumerate(rows):
  if float(first['active_wall_s'])<5:continue
  for last in rows[i+1:]:
   delta=float(last['active_wall_s'])-float(first['active_wall_s'])
   if delta>=5:windows.append((float(last['time_s'])-float(first['time_s']))/delta);break
 for f in set(profile.glob('*'))-old:
  if f.is_file():shutil.copy2(f,out/f.name)
 summary={'coupled_rectangular_gas_experiment':a.coupled,'target_rate':a.rate,'requested_physical_s':a.seconds,'result':result,'five_second_windows':{'count':len(windows),'min':min(windows),'median':statistics.median(windows),'max':max(windows)},'max_temperature_c':max(float(s['gpu'].split(',')[5]) for s in samples),'max_power_w':max(float(s['gpu'].split(',')[2]) for s in samples),'initial_identical':True,'scope':'1280x720 30FPS cap; existing editor left open; observed run only, not guaranteed future minimum'}
 (out/'summary.json').write_text(json.dumps(summary,indent=2));print(json.dumps(summary,indent=2),flush=True)
if __name__=='__main__':main()
