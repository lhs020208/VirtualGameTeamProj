from pathlib import Path
import subprocess,json,time
root=Path('D:/GitHub/VirtualGameTeamProj');out=root/'Docs/DEM/optimization37_44';out.mkdir(parents=True,exist_ok=True)
exe=root/'Intermediate/PowderCore/Release/granular_run.exe'
cases=[('base30',30000,4,16,.00025,30,1e9,0),('n100',100000,3,16,.00025,30,1e9,0),('n300',300000,1.5,16,.00025,30,1e9,0),('n1000',1000000,1.25,16,.00025,30,1e9,0),('dt_half',30000,4,16,.000125,30,1e9,0),('iter24',30000,4,24,.00025,30,1e9,0),('stopped',30000,4,16,.00025,0,1e9,0),('fixed_r40',40000,2,16,.00025,30,1e9,.00754619)]
results=[]
for label,n,duration,it,dt,rpm,gate,radius in cases:
 folder=out/label;assert not folder.exists()
 with (out/(label+'.log')).open('w') as log:
  start=time.monotonic();p=subprocess.Popen([str(exe),str(n),str(duration),str(folder),str(it),str(dt),str(rpm),str(gate),str(radius)],stdout=log,stderr=subprocess.STDOUT)
  try:code=p.wait(timeout=300);status='completed' if code==0 else 'rejected'
  except subprocess.TimeoutExpired:p.kill();p.wait();code=-1;status='measurement_budget_exceeded'
 result=dict(label=label,count=n,duration=duration,iterations=it,dt=dt,rpm=rpm,radius_override=radius,exit_code=code,status=status,wall=time.monotonic()-start)
 results.append(result);(out/'scale_manifest.json').write_text(json.dumps(results,indent=2));print(json.dumps(result),flush=True)
