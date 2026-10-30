"""Blender background: independent source-wall clearance along gas routes."""
import json, math
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
root=Path(__file__).resolve().parents[1]/'SourceAssets/ProcessPlantPhysics'
model=json.loads((root/'runtime_manifest.json').read_text(encoding='utf-8'))
vertices=[];triangles=[];owners=[]
for name in model['selected_meshes']:
    d=json.loads((root/(name+'.json')).read_text(encoding='utf-8'));v=d['vertices']
    if name=='Collision_WH101_Inspection_Lid':v=[[p[0]+.195,p[1]+.195,p[2]+1.837] for p in v]
    offset=len(vertices);vertices.extend(v);triangles.extend([[i+offset for i in t] for t in d['triangles']]);owners.extend([name]*len(d['triangles']))
bvh=BVHTree.FromPolygons(vertices,triangles,all_triangles=True)
errors=[];minimum=100;tested=0
for route in model['routes']:
    segments=list(zip(route['points'],route['points'][1:]))
    if 'box_cell' in route:
        a,b=route['box_cell']
        # The box's outer end is a closed wall, not an inlet. Its supply is
        # the vertical pipe above the chamber; audit the actual turning path.
        entry=[*route['points'][-1][:2],a[2],a[3]]
        segments.extend([(route['points'][-1],entry),(entry,b)])
    for a,b in segments:
        n=max(1,math.ceil(math.dist(a[:3],b[:3])/.0025))
        for j in range(n+1):
            p=Vector(a[:3]).lerp(Vector(b[:3]),j/n);q,normal,face,dist=bvh.find_nearest(p);tested+=1;minimum=min(minimum,dist)
            if dist<.00125:errors.append(dict(route=route['id'],point=list(p),distance=dist,wall=owners[face]))
report=dict(tested=tested,minimum_clearance_m=minimum,obstructions=errors,passed=not errors,note='Centreline clearance only, not full fluid-volume certification or particle transport proof')
(root/'route_clearance.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('ROUTE_AUDIT',tested,'minimum',minimum,'obstructed',len(errors))
for x in errors[:20]:print(x)
