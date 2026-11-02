"""Reproducible 30-point surface-location workloads, not physical trajectories."""
from pathlib import Path
import json
import numpy as np

root=Path(__file__).resolve().parents[1]
src=root/'SourceAssets/ProcessPlantPhysics'
out=root/'Docs/ProcessPlantPhysics/hybrid/profile'
out.mkdir(exist_ok=True,parents=True)
manifest=json.loads((src/'runtime_manifest.json').read_text())
groups={}
charge=np.loadtxt(root/'SourceAssets/ProcessPlantHybrid/test_charge.csv',delimiter=',',skiprows=1,usecols=(3,4,5))
groups['actual_initial_30']=charge
groups['far_empty_30']=np.tile([8.,8.,8.],(30,1))
for name in manifest['selected_meshes']+['MX-301_Collision_Rotor']:
    data=json.loads((src/(name+'.json')).read_text())
    v=np.asarray(data['vertices']);t=np.asarray(data['triangles'])
    if name=='Collision_WH101_Inspection_Lid':v=v+[.195,.195,1.837]
    triangles=v[t];normal=np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0]);area=np.linalg.norm(normal,axis=1)
    valid=area>1e-12;triangles=triangles[valid];normal=normal[valid]/area[valid,None];area=area[valid]
    # Equal area quantiles prevent tiny decorative triangles dominating probes.
    indices=np.searchsorted(np.cumsum(area),area.sum()*(np.arange(30)+.5)/30)
    groups[name]=triangles[indices].mean(axis=1)+.001*normal[indices]
with (out/'wall_probes.txt').open('w') as f:
    for name,points in groups.items():
        f.write(f'{name} {len(points)}\n')
        np.savetxt(f,points,fmt='%.9g')
print(len(groups),'groups, 30 probes each')

# Diagnostic ablation IDs: move only these shapes in the profiler's private copy.
# Never write a modified runtime.plant or use this ablation as a simulation.
hybrid=root/'SourceAssets/ProcessPlantHybrid'
packed=np.frombuffer((hybrid/'runtime.plant').read_bytes(),dtype='<f4',offset=4).reshape(-1,4)
geometry=json.loads((hybrid/'obb_geometry.json').read_text())
keys=set()
for shape in geometry['static']:
    if shape['source']=='Collision_WH101_Separate_Top_Cover':
        keys.add(np.asarray([[*shape['centre'],0]]+[[*a,h] for a,h in zip(shape['axes'],shape['half'])],dtype='<f4').tobytes())
ids=[]
for at in range(int(packed[12,0]),int(packed[13,0]),2):
    if packed[at,3]>=0:continue
    start=int(packed[at+1,3]);count=-int(packed[at,3])
    for ref in packed[start:start+count]:
        k=int(ref[0])
        if ref[1]!=1 and packed[k:k+4].tobytes() in keys:ids.append(k)
assert len(ids)==len(keys),'mask must match every cover patch'
(out/'cover_mask.txt').write_text('\n'.join(f'{k} 2' for k in ids))
