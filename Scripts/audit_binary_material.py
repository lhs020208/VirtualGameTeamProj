"""Independent heterogeneous-radius/mass audit. No run parameter changes."""
from pathlib import Path
import sys,json,csv,math
import numpy as np
from scipy.spatial import cKDTree
root=Path(__file__).resolve().parents[1]
folder=Path(sys.argv[1])
model=json.loads((folder/'material_model.json').read_text())
materials=model['materials'];geometry=json.loads((root/'SourceAssets/Granular/MX301_convex.json').read_text())
initial=np.fromfile(folder/'initial.bin',dtype='<f4').reshape(-1,4)
rows=list(csv.DictReader((folder/'trace.csv').open()))
samples=[]
cases=[(folder/'final.bin',float(rows[-1]['angle_rad']),float(rows[-1]['time_s']))]
for f in sorted((folder/'audit').glob('*.json')):
 meta=json.loads(f.read_text());cases.append((f.with_suffix('.bin'),meta['angle_rad'],meta['time_s']))
for file,angle,t in cases:
 p=np.fromfile(file,dtype='<f4').reshape(-1,4).astype(float)
 assert len(p)==len(initial) and np.isfinite(p).all() and np.array_equal(p[:,3],initial[:,3])
 g=p[:,3].astype(int);r=np.array([m['radius_m'] for m in materials])[g];mass=np.array([m['particle_mass_kg'] for m in materials])[g]
 xyz=p[:,:3];pairs=cKDTree(xyz).query_pairs(2*r.max(),output_type='ndarray')
 a,b=pairs.T;overlap=float(np.maximum((r[a]+r[b]-np.linalg.norm(xyz[a]-xyz[b],axis=1))/np.minimum(r[a],r[b]),0).max(initial=0))
 wall=0.;rotor=0.
 for plane in geometry['interior'][0]['planes']:
  plane=np.array(plane);wall=max(wall,float(np.maximum(xyz@plane[:3]-plane[3]+r,0).max(initial=0)))
 local=xyz.copy();local[:,1]=np.cos(angle)*xyz[:,1]+np.sin(angle)*xyz[:,2];local[:,2]=-np.sin(angle)*xyz[:,1]+np.cos(angle)*xyz[:,2]
 for part in geometry['rotor']:
  bounds=np.array(part['bounds']);mask=((local>=bounds[:,0]-r[:,None])&(local<=bounds[:,1]+r[:,None])).all(axis=1);v=local[mask]
  if not len(v):continue
  d=np.full(len(v),-np.inf)
  for plane in np.array(part['planes']):d=np.maximum(d,v@plane[:3]-plane[3])
  rotor=max(rotor,float(np.maximum(r[mask]-d,0).max(initial=0)))
 bins=np.clip(((xyz+[.75,.45,.325])/[1.5,.9,.65]*4).astype(int),0,3);ids=(bins[:,2]*4+bins[:,1])*4+bins[:,0]
 total=np.bincount(ids,weights=mass,minlength=64);red=np.bincount(ids,weights=mass*(g==0),minlength=64);keep=total>0;fraction=mass[g==0].sum()/mass.sum()
 mix=1-np.sum(total[keep]/mass.sum()*(red[keep]/total[keep]-fraction)**2)/(fraction*(1-fraction))
 if file.name=='final.bin':assert abs(mix-float(rows[-1]['mix_index']))<1.e-5
 samples.append(dict(time_s=t,overlap_min_radius=overlap,wall_m=wall,rotor_m=rotor,mass_kg=float(mass.sum()),mix_index=float(mix),pass_geometry=overlap<=.1 and wall<=1.e-4 and rotor<=1.e-4))
 result=json.loads((folder/'result.json').read_text(encoding='utf-8-sig'))
 report=dict(result=result,samples=samples,passed=all(s['pass_geometry'] for s in samples) and not result['error'],calibrated=False,scope='sampled geometry and mass; not empirical validation or guaranteed speed')
(folder/'audit_report.json').write_text(json.dumps(report,indent=2))
print(json.dumps(dict(result=result,passed=report['passed'],max_overlap=max(s['overlap_min_radius'] for s in samples),samples=len(samples)),indent=2))
sys.exit(0 if report['passed'] else 1)
