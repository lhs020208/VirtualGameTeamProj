from pathlib import Path
import json,csv,math,shutil
import numpy as np
from scipy.spatial import cKDTree
root=Path(__file__).resolve().parents[1]
out=root/'Docs/DEM/emitter_integration06';out.mkdir(exist_ok=True)
geometry=json.loads((root/'SourceAssets/Granular/MX301_convex.json').read_text())
def boxpenetration(p,h,r):
 q=np.abs(p)-h;d=np.linalg.norm(np.maximum(q,0),axis=1)+np.minimum(q.max(axis=1),0)
 return float(np.maximum(r-d,0).max(initial=0))
def geometry_audit(p,r,angle,gate,mx):
 vessel=0.;rotor=0.
 if mx:
  for plane in geometry['interior'][0]['planes']:
   a=np.array(plane);vessel=max(vessel,float(np.maximum(p@a[:3]-a[3]+r,0).max(initial=0)))
  local=p.copy();local[:,1]=np.cos(angle)*p[:,1]+np.sin(angle)*p[:,2];local[:,2]=-np.sin(angle)*p[:,1]+np.cos(angle)*p[:,2]
  for part in geometry['rotor']:
   bounds=np.array(part['bounds']);mask=((local>=bounds[:,0]-r)&(local<=bounds[:,1]+r)).all(axis=1);v=local[mask]
   if not len(v):continue
   distance=np.full(len(v),-np.inf)
   for a in np.array(part['planes']):distance=np.maximum(distance,v@a[:3]-a[3])
   rotor=max(rotor,float(np.maximum(r-distance,0).max(initial=0)))
 else:
  main=p[:,2]>=0;recv=p[:,2]<-.25
  for v in [np.abs(p[main|recv,0])-.45+r,np.abs(p[main|recv,1])-.25+r,p[main,2]-.4+r,-.6+r-p[recv,2]]:vessel=max(vessel,float(np.maximum(v,0).max(initial=0)))
  for side in [-1,1]:
   for centre,half in [([side*.255,0,-.01],[.195,.25,.01]),([0,side*.155,-.01],[.06,.095,.01]),([side*.07,0,-.125],[.01,.08,.125]),([0,side*.07,-.125],[.06,.01,.125])]:vessel=max(vessel,boxpenetration(p-centre,half,r))
  if not gate:vessel=max(vessel,boxpenetration(p-[0,0,-.01],[.06,.06,.01],r))
  rotor=float(np.maximum(.025+r-np.linalg.norm(p[:,1:]-[0,.21],axis=1),0).max(initial=0))
  for k in range(6):
   v=p-[-.375+k*.15,0,.21];a=angle+k*math.pi/3
   local=v.copy();local[:,1]=np.cos(a)*v[:,1]+np.sin(a)*v[:,2]-.095;local[:,2]=-np.sin(a)*v[:,1]+np.cos(a)*v[:,2]
   b=-.4 if k%2 else .4;v=local.copy();v[:,0]=np.cos(b)*local[:,0]+np.sin(b)*local[:,1];v[:,1]=-np.sin(b)*local[:,0]+np.cos(b)*local[:,1]
   rotor=max(rotor,boxpenetration(v,[.045,.07,.012],r))
 return vessel,rotor

report={'physical_accuracy_validated':False,'results':{}}
reference=(root/'Docs/DEM/emitter_integration05/reference_initial/initial.bin').read_bytes()
for label in ['baseline12','integrated12','integrated30']:
 source=root/'Saved'/('Emitter06_'+label)
 if not (source/'result.json').exists():continue
 result=json.loads((source/'result.json').read_text(encoding='utf-8-sig'))
 assert not result['error']
 assert (source/'initial.bin').read_bytes()==reference
 rows=list(csv.DictReader((source/'trace.csv').open()));last=rows[-1]
 p=np.fromfile(source/'final.bin',dtype=np.float32).reshape(-1,4)
 assert len(p)==50000 and np.isfinite(p).all()
 initial=np.frombuffer(reference,dtype=np.float32).reshape(-1,4)
 assert np.array_equal(p[:,3],initial[:,3])
 r=float(np.float32((.054/(50000*4*math.pi/3))**(1/3)))
 pairs=cKDTree(p[:,:3]).query_pairs(2*r,output_type='ndarray')
 overlap=float(np.maximum(2*r-np.linalg.norm(p[pairs[:,0],:3]-p[pairs[:,1],:3],axis=1),0).max(initial=0)/r)
 vessel,rotor=geometry_audit(p[:,:3].astype(np.float64),r,float(last['angle_rad']),False,True)
 assert overlap<=.1,(label,overlap)
 assert vessel<=.0001 and rotor<=.0001,(label,vessel,rotor)
 assert all(abs(float(x['mass_kg'])-64.8)<.0001 for x in rows)
 result.update(snapshot_time=float(last['time_s']),final_overlap_radius=overlap,final_vessel_penetration_m=vessel,final_rotor_penetration_m=rotor,initial_byte_identical=True,mass_kg=float(last['mass_kg']),groups=[25000,25000],final_mean_z=float(p[:,2].mean()),final_mixing_index=float(last['mix_index']),target_rate_pass=result['rate']>=.1)
 report['results'][label]=result
 dest=out/label;dest.mkdir(exist_ok=True)
 for name in ['settings.json','result.json','trace.csv','final.bin','mix.png','discharge.png']:
  if (source/name).exists():shutil.copy2(source/name,dest/name)
(out/'acceptance.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
