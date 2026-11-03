"""Read-only static wall candidate audit; saved positions, not a live GPU trace."""
from pathlib import Path
from collections import Counter
import json
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
src=ROOT/'SourceAssets/ProcessPlantPhysics'; hybrid=ROOT/'SourceAssets/ProcessPlantHybrid'
out=ROOT/'Docs/ProcessPlantPhysics/CollisionAudit';out.mkdir(parents=True,exist_ok=True)
manifest=json.loads((src/'runtime_manifest.json').read_text())
data=np.fromfile(hybrid/'runtime.plant',dtype='<f4',offset=4).reshape(-1,4)
lookup={}
def key(t):return b''.join(sorted(np.asarray(t,dtype='<f4').reshape(3,3)[i].tobytes() for i in range(3)))
for name in manifest['selected_meshes']:
    mesh=json.loads((src/(name+'.json')).read_text());v=np.asarray(mesh['vertices'])
    if name=='Collision_WH101_Inspection_Lid':v=v+[.195,.195,1.837]
    for tri in v[np.asarray(mesh['triangles'])]:lookup[key(tri)]=name
boxes={}
for s in json.loads((hybrid/'obb_geometry.json').read_text())['static']:
    boxes[np.asarray([[*s['centre'],0]]+[[*a,h] for a,h in zip(s['axes'],s['half'])],dtype='<f4').tobytes()]=s['source']
leaves=[];inventory=Counter();bounds={};names={}
for at in range(int(data[12,0]),int(data[13,0]),2):
    if data[at,3]>=0:continue
    ids=[]
    for ref in data[int(data[at+1,3]):int(data[at+1,3])-int(data[at,3])]:
        k,typ=map(int,ref[:2]);ids.append(k)
        if typ==1:
            vertices=data[k:k+3,:3];name=lookup.get(key(vertices),'generated_connection_sleeve');lo,hi=vertices.min(0),vertices.max(0)
        else:
            name=boxes[data[k:k+4].tobytes()];c=data[k,:3];e=data[k+1:k+4,3]@abs(data[k+1:k+4,:3]);lo,hi=c-e,c+e
        inventory[name]+=1;names[k]=name;bounds[k]=(lo,hi)
    leaves.append((data[at,:3],data[at+1,:3],ids))
lo=np.array([a for a,b,c in leaves]);hi=np.array([b for a,b,c in leaves])
result={'method':'Static original leaf AABB overlap at frozen sampled positions, radius 2.004 mm. Counts shape function candidates before projection; not measured GPU calls, not whole-trajectory reachability. Rotor/gates/reference fallback not included.','inventory':dict(inventory),'samples':{}}
for label,path,cols in [('initial',hybrid/'test_charge.csv',(3,4,5)),('recorded_stall',ROOT/'Docs/ProcessPlantPhysics/BV122Inspection/live_a.csv',(2,3,4))]:
    p=np.loadtxt(path,delimiter=',',skiprows=1,usecols=cols);p=p[np.linspace(0,len(p)-1,min(512,len(p)),dtype=int)]
    calls=Counter();misses=Counter();radius=.002004
    for point in p:
        hits=np.flatnonzero(np.all(point>=lo-radius,axis=1)&np.all(point<=hi+radius,axis=1))
        for leaf in hits:
            for k in leaves[leaf][2]:
                name=names[k];calls[name]+=1;a,b=bounds[k]
                if np.any(point<a-radius) or np.any(point>b+radius):misses[name]+=1
    result['samples'][label]={'points':len(p),'shape_candidates':sum(calls.values()),'individual_aabb_misses':sum(misses.values()),'per_source':[{'name':n,'candidates':c,'individual_aabb_misses':misses[n]} for n,c in calls.most_common()]}
(out/'candidates.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in result.items() if k!='inventory'},indent=2))
