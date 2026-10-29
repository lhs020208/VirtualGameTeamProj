"""Headless current-map air wiring smoke test. Does not save assets or certify FPS."""
from pathlib import Path
import subprocess,time,json,datetime
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Saved'/('PrescribedAirUnreal_'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ'))
REPORT=ROOT/'Docs/DEM/air_extension'
def main():
 OUT.mkdir(exist_ok=False)
 cmd=['C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe',str(ROOT/'VirtualGameTeamProj.uproject'),'/Game/DEM/Tests/L_MX301_BinaryMaterials','-game','-nullrhi','-unattended','-nosound','-nop4','-ddc=InstalledNoZenLocalFallback','-GrainBenchmark','-GrainSeconds=2','-GrainPrescribedAir','-GrainAirX=2','-GrainAuditSnapshots',f'-GrainOutput={OUT}',f'-abslog={OUT}/engine.log']
 samples=[];start=time.perf_counter()
 with (OUT/'console.log').open('w') as log:
  proc=subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
  try:
   while proc.poll() is None:
    if time.perf_counter()-start>180:raise RuntimeError('own UE smoke timeout')
    q=subprocess.run(['nvidia-smi','--query-gpu=temperature.gpu,power.draw','--format=csv,noheader,nounits'],capture_output=True,text=True,timeout=5,creationflags=subprocess.CREATE_NO_WINDOW)
    v=[float(x) for x in q.stdout.strip().split(',')] if q.returncode==0 else []
    if len(v)!=2 or v[0]>=75 or v[1]>=155:raise RuntimeError('conservative hardware stop / telemetry missing')
    samples.append(v);time.sleep(.5)
   assert proc.returncode==0,proc.returncode
  finally:
   if proc.poll() is None:proc.terminate();proc.wait(timeout=30)
 air=json.loads((OUT/'air_model.json').read_text());assert air['enabled'] and air['velocity_local_m_s']==[2,0,0]
 subprocess.run(['python',str(ROOT/'Scripts/audit_binary_material.py'),str(OUT)],check=True)
 audit=json.loads((OUT/'audit_report.json').read_text());assert audit['passed']
 (REPORT/'unreal_smoke.json').write_text(json.dumps(dict(command=cmd,air=air,audit=audit,max_temperature_c=max(x[0] for x in samples),max_power_w=max(x[1] for x in samples),rendered=False),indent=2))
 print('PASS current map air worker metadata and geometry',flush=True)
if __name__=='__main__':main()
