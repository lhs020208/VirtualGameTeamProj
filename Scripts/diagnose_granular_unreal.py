"""Compare the committed UE runtime with rendering enabled / NullRHI.
Existing editor is left untouched and remains a background-load caveat.
"""
from pathlib import Path
import subprocess,time,json,psutil,hashlib
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Saved/HardwareDiagnosis/unreal'
OUT.mkdir(parents=True,exist_ok=True)
for label,extra in [('render_default',[]),('render30',['-ExecCmds=t.MaxFPS 30']),('nullrhi',['-nullrhi','-ExecCmds=t.MaxFPS 30'])]:
 dest=OUT/label
 if dest.exists():raise RuntimeError('output exists')
 dest.mkdir()
 cmd=['C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe',str(ROOT/'VirtualGameTeamProj.uproject'),'/Game/DEM/Tests/L_MX301_BinaryMaterials','-game','-windowed','-ResX=1280','-ResY=720','-unattended','-nosound','-nop4','-ddc=InstalledNoZenLocalFallback','-GrainBenchmark','-GrainSeconds=12','-GrainAuditSnapshots',f'-GrainOutput={dest}',f'-abslog={dest}/engine.log',*extra]
 (dest/'command.json').write_text(json.dumps(cmd,indent=2));samples=[];start=time.perf_counter()
 with (dest/'console.log').open('w') as log:
  proc=subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW);child=psutil.Process(proc.pid)
  while proc.poll() is None:
   if time.perf_counter()-start>300:proc.terminate();raise RuntimeError('own benchmark timeout')
   try:
    cpu=child.cpu_times();gpu=subprocess.run(['nvidia-smi','--query-gpu=utilization.gpu,utilization.memory,power.draw,memory.used,clocks.sm,temperature.gpu','--format=csv,noheader,nounits'],capture_output=True,text=True,creationflags=subprocess.CREATE_NO_WINDOW,timeout=5)
    values=[float(v.strip()) for v in gpu.stdout.strip().split(',')] if gpu.returncode==0 and gpu.stdout.strip() else []
    if len(values)!=6 or values[5]>=75 or values[2]>=155:
     proc.terminate();proc.wait(timeout=30);raise RuntimeError('Conservative hardware stop: telemetry unavailable, temperature >=75C, or power >=155W')
    samples.append({'elapsed_s':time.perf_counter()-start,'process_cpu_s':cpu.user+cpu.system,'gpu':gpu.stdout.strip()})
   except psutil.NoSuchProcess:break
   except Exception:
    if proc.poll() is None:proc.terminate();proc.wait(timeout=30)
    raise
   time.sleep(.5)
  code=proc.wait()
 (dest/'monitor.json').write_text(json.dumps(samples,indent=2))
 if code or not (dest/'result.json').exists():raise RuntimeError(f'{label} failed {code}')
 if hashlib.sha256((dest/'initial.bin').read_bytes()).hexdigest()!='9547042760b376c83d3820068abe4e52b4093235062f4476a97c3fbb1cb8e29d':raise RuntimeError('fixture changed')
 print(label,(dest/'result.json').read_text(encoding='utf-8-sig'),flush=True)
