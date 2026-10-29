"""Current 50k fixture only; prescribed uniform air is a cost experiment, not dense-flow validation."""
from pathlib import Path
import subprocess, time, json, hashlib, statistics
import numpy as np
from scipy.spatial import cKDTree
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Saved/PrescribedAirValidation'
REPORT=ROOT/'Docs/DEM/air_extension'
INITIAL=ROOT/'Saved/Rate01Recheck/current12/initial.bin'
EXPECTED='9547042760b376c83d3820068abe4e52b4093235062f4476a97c3fbb1cb8e29d'
def audit(path,angle):
 p=np.fromfile(path,dtype='<f4').reshape(-1,4).astype(float)
 initial=np.fromfile(INITIAL,dtype='<f4').reshape(-1,4)
 assert p.shape==initial.shape and np.isfinite(p).all() and np.array_equal(p[:,3],initial[:,3])
 r=np.where(p[:,3]==0,.0063647064,.005);xyz=p[:,:3]
 pairs=cKDTree(xyz).query_pairs(2*r.max(),output_type='ndarray');a,b=pairs.T
 overlap=float(np.maximum((r[a]+r[b]-np.linalg.norm(xyz[a]-xyz[b],axis=1))/np.minimum(r[a],r[b]),0).max(initial=0))
 geom=json.loads((ROOT/'SourceAssets/Granular/MX301_convex.json').read_text());wall=rotor=0.
 for plane in np.array(geom['interior'][0]['planes']):wall=max(wall,float(np.maximum(xyz@plane[:3]-plane[3]+r,0).max(initial=0)))
 local=xyz.copy();local[:,1]=np.cos(angle)*xyz[:,1]+np.sin(angle)*xyz[:,2];local[:,2]=-np.sin(angle)*xyz[:,1]+np.cos(angle)*xyz[:,2]
 for part in geom['rotor']:
  bounds=np.array(part['bounds']);mask=((local>=bounds[:,0]-r[:,None])&(local<=bounds[:,1]+r[:,None])).all(axis=1)
  if not mask.any():continue
  dist=np.full(mask.sum(),-np.inf)
  for plane in np.array(part['planes']):dist=np.maximum(dist,local[mask]@plane[:3]-plane[3])
  rotor=max(rotor,float(np.maximum(r[mask]-dist,0).max(initial=0)))
 mass=float(np.sum(4*np.pi/3*r**3*np.where(p[:,3]==0,900,1500)))
 return dict(overlap_min_radius=overlap,wall_m=wall,rotor_m=rotor,mass_kg=mass,passed=overlap<=.1 and wall<=1e-4 and rotor<=1e-4)
def run():
 assert hashlib.sha256(INITIAL.read_bytes()).hexdigest()==EXPECTED
 OUT.mkdir(exist_ok=False);REPORT.mkdir(exist_ok=True)
 results=[]
 for label,mode in [('off1','off'),('still1','still'),('wind1','wind'),('wind2','wind'),('still2','still'),('off2','off')]:
  dest=OUT/label
  cmd=[str(ROOT/'Intermediate/PowderCore/Release/granular_diagnose.exe'),str(INITIAL),str(dest),'10688','32','128','0','50','0',mode]
  samples=[];start=time.perf_counter()
  with (REPORT/(label+'.log')).open('w') as log:
   proc=subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
   try:
    while proc.poll() is None:
     if time.perf_counter()-start>180:raise RuntimeError('own benchmark timeout')
     g=subprocess.run(['nvidia-smi','--query-gpu=temperature.gpu,power.draw,utilization.gpu','--format=csv,noheader,nounits'],capture_output=True,text=True,timeout=5,creationflags=subprocess.CREATE_NO_WINDOW)
     values=[float(x.strip()) for x in g.stdout.strip().split(',')] if g.returncode==0 else []
     if len(values)!=3 or values[0]>=75 or values[1]>=155:raise RuntimeError('conservative hardware stop or missing telemetry')
     samples.append(values);time.sleep(.5)
    if proc.returncode:raise RuntimeError(label+' failed; see log')
   finally:
    if proc.poll() is None:proc.terminate();proc.wait(timeout=30)
  result=json.loads((dest/'result.json').read_text());result.update(label=label,mode=mode,command=cmd,audit=audit(dest/'final.bin',result['angle_rad']),max_temperature_c=max(x[0] for x in samples),max_power_w=max(x[1] for x in samples))
  (REPORT/(label+'_monitor.json')).write_text(json.dumps(samples,indent=2));results.append(result)
  (REPORT/'performance.json').write_text(json.dumps(dict(initial_sha256=EXPECTED,runs=results),indent=2))
  print(label,result['rate'],result['audit'],flush=True)
  assert result['audit']['passed']
 rates={mode:statistics.median(r['rate'] for r in results if r['mode']==mode) for mode in ('off','still','wind')}
 report=dict(initial_sha256=EXPECTED,runs=results,median_rate=rates,cost_ratio={m:rates['off']/rates[m] for m in ('still','wind')},scope='4.008s per run, current 50k virtual spheres, solver only with editor open; final geometry samples; not 30FPS or physical dense-flow certification')
 (REPORT/'performance.json').write_text(json.dumps(report,indent=2));print(json.dumps(rates))
if __name__=='__main__':run()
