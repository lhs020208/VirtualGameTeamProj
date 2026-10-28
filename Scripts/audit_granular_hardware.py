"""Audit the final states of hardware probes with heterogeneous radii.
Does not certify unsampled steps, trajectory equality, or empirical material accuracy.
"""
from pathlib import Path
import json,sys
import numpy as np
from scipy.spatial import cKDTree
ROOT=Path(__file__).resolve().parents[1]
G=json.loads((ROOT/'SourceAssets/Granular/MX301_convex.json').read_text())
initial=np.fromfile(ROOT/'Saved/Rate01Recheck/current12/initial.bin',dtype='<f4').reshape(-1,4)
reports={}
for folder in Path(sys.argv[1]).iterdir():
 if not (folder/'result.json').is_file():continue
 result=json.loads((folder/'result.json').read_text(encoding='utf-8-sig'))
 p=np.fromfile(folder/'final.bin',dtype='<f4').reshape(-1,4).astype(float)
 assert p.shape==initial.shape and np.isfinite(p).all() and np.array_equal(p[:,3],initial[:,3])
 r=np.where(p[:,3]==0,.0063647064,.005);xyz=p[:,:3]
 pairs=cKDTree(xyz).query_pairs(2*r.max(),output_type='ndarray');a,b=pairs.T
 overlap=float(np.maximum((r[a]+r[b]-np.linalg.norm(xyz[a]-xyz[b],axis=1))/np.minimum(r[a],r[b]),0).max(initial=0))
 wall=0.;rotor=0.;angle=result['angle_rad']
 for plane in G['interior'][0]['planes']:
  plane=np.array(plane);wall=max(wall,float(np.maximum(xyz@plane[:3]-plane[3]+r,0).max(initial=0)))
 local=xyz.copy();local[:,1]=np.cos(angle)*xyz[:,1]+np.sin(angle)*xyz[:,2];local[:,2]=-np.sin(angle)*xyz[:,1]+np.cos(angle)*xyz[:,2]
 for part in G['rotor']:
  bounds=np.array(part['bounds']);mask=((local>=bounds[:,0]-r[:,None])&(local<=bounds[:,1]+r[:,None])).all(axis=1);v=local[mask]
  if not len(v):continue
  d=np.full(len(v),-np.inf)
  for plane in np.array(part['planes']):d=np.maximum(d,v@plane[:3]-plane[3])
  rotor=max(rotor,float(np.maximum(r[mask]-d,0).max(initial=0)))
 reports[folder.name]={'count':len(p),'group_counts':[int((p[:,3]==i).sum()) for i in range(2)],'max_overlap_min_radius':overlap,'wall_m':wall,'rotor_m':rotor,'passed':overlap<=.1 and wall<=1.e-4 and rotor<=1.e-4,'scope':'final sample only; not trajectory equality or real-material validation'}
print(json.dumps(reports,indent=2));(Path(sys.argv[1])/'geometry_audit.json').write_text(json.dumps(reports,indent=2))
if not reports or not all(r['passed'] for r in reports.values()):sys.exit(1)
