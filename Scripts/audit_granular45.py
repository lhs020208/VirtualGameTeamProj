from pathlib import Path
import numpy as np
import csv,json,math
root=Path('D:/GitHub/VirtualGameTeamProj');out=root/'Docs/DEM/optimization45'
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
report={'physical_accuracy_validated':False,'cases':{}}
for name,mx in [('dt375',False),('dt500_i24',False),('mx301_12',True),('mx301_dt500',True),('candidate_long',False),('verify100',False),('verify100_dt350',False),('mx301_dt375',True)]:
 folder=out/name
 if not (folder/'timing.json').exists():continue
 rows=list(csv.DictReader((folder/'trace.csv').open()));count=int(rows[-1]['count']);r=(.054/(count*4*math.pi/3))**(1/3)
 assert all(math.isfinite(float(v)) for row in rows for v in row.values())
 first=None;records=[]
 frames=list(folder.glob('frame_*.bin'))+list((root/'Saved/Optimization45Snapshots'/name).glob('frame_*.bin'))+ [folder/'final.bin']
 for f in frames:
  p=np.fromfile(f,dtype=np.float32).reshape(-1,4);assert len(p)==count and np.isfinite(p).all()
  if first is None:first=p[:,3].copy()
  assert np.array_equal(first,p[:,3]),'group identity changed'
  if f.name=='final.bin':t=float(rows[-1]['time']);angle=float(rows[-1]['angle'])
  else:t,angle=map(float,f.stem.split('_')[1:])
  wall,rotor=geometry_audit(p[:,:3].astype(float),r,angle,t>=9,mx)
  records.append({'time':t,'vessel_penetration_m':wall,'rotor_penetration_m':rotor})
 maximum=max(float(row['max_overlap_radius']) for row in rows)
 report['cases'][name]={'last':rows[-1],'max_pair_overlap_radius':maximum,'max_sampled_vessel_penetration_m':max(x['vessel_penetration_m'] for x in records),'max_sampled_rotor_penetration_m':max(x['rotor_penetration_m'] for x in records),'samples':records,'groups_preserved':True,'pair_gate_pass':maximum<=.1}
(out/'geometry_audit.json').write_text(json.dumps(report,indent=2))
print(json.dumps({k:{a:b for a,b in v.items() if a!='samples'} for k,v in report['cases'].items()},indent=2))
