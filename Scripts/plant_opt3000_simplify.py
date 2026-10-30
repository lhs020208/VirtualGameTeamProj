"""Offline planar-only collision experiment; source/display assets untouched."""
import bpy,bmesh,json,struct,math
import numpy as np
from pathlib import Path
root=Path('D:/GitHub/VirtualGameTeamProj');src=root/'SourceAssets/ProcessPlantPhysics';dst=root/'Docs/ProcessPlantPhysics/optimization3000/simplified';dst.mkdir(exist_ok=True)
m=json.loads((src/'runtime_manifest.json').read_text(encoding='utf-8'));triangles=[];stats=[]
for name in m['selected_meshes']:
 d=json.loads((src/(name+'.json')).read_text(encoding='utf-8'));mesh=bpy.data.meshes.new(name);mesh.from_pydata(d['vertices'],[],d['triangles']);bm=bmesh.new();bm.from_mesh(mesh)
 bmesh.ops.dissolve_limit(bm,angle_limit=0.00001,verts=list(bm.verts),edges=list(bm.edges),use_dissolve_boundaries=False)
 bmesh.ops.triangulate(bm,faces=list(bm.faces));bm.verts.ensure_lookup_table();ts=np.array([[list(v.co) for v in f.verts] for f in bm.faces]);bm.free();bpy.data.meshes.remove(mesh)
 if name=='Collision_WH101_Inspection_Lid':ts+=np.array([.195,.195,1.837])
 triangles.extend(ts.tolist());stats.append([name,len(d['triangles']),len(ts)])
for entry in m['test_overrides']:
 if entry['kind']!='open_sleeve':continue
 a=np.array(entry['a']);b=np.array(entry['b']);axis=b-a;axis/=np.linalg.norm(axis);u=np.cross(axis,[0,0,1] if abs(axis[2])<.9 else [0,1,0]);u/=np.linalg.norm(u);v=np.cross(axis,u)
 rings=[[p+r*(u*math.cos(j*math.tau/64)+v*math.sin(j*math.tau/64)) for j in range(64)] for p,r in [(a,entry['radius_a_m']),(b,entry['radius_b_m'])]]
 for j in range(64):
  k=(j+1)%64;triangles.extend([[rings[0][j],rings[1][j],rings[1][k]],[rings[0][j],rings[1][k],rings[0][k]]])
f=(src/'runtime.plant').read_bytes();packed=np.frombuffer(f,dtype='<f4',offset=4).reshape(-1,4).tolist();ts=np.array(triangles);ts=ts[np.linalg.norm(np.cross(ts[:,1]-ts[:,0],ts[:,2]-ts[:,0]),axis=1)>1e-12]
def node(ids):
 at=len(packed);packed.extend([[0.]*4,[0.]*4]);t=ts[ids];lo=t.min(axis=(0,1));hi=t.max(axis=(0,1))
 if len(ids)<=8:
  start=len(packed);packed.extend([list(v)+[0.] for tri in t for v in tri]);packed[at]=list(lo)+[-len(ids)];packed[at+1]=list(hi)+[start]
 else:
  centres=t.mean(axis=1);axis=int(np.argmax(np.ptp(centres,axis=0)));order=np.argsort(centres[:,axis],kind='stable');mid=len(ids)//2;left=node(ids[order[:mid]]);right=node(ids[order[mid:]]);packed[at]=list(lo)+[left];packed[at+1]=list(hi)+[right]
 return at
packed[0][0]=node(np.arange(len(ts)));(dst/'runtime.plant').write_bytes(struct.pack('<I',len(packed))+np.asarray(packed,dtype='<f4').tobytes())
for name in ['runtime.routes','test_charge.csv']:(dst/name).write_bytes((src/name).read_bytes())
(dst/'stats.json').write_text(json.dumps({'before':m['static_triangles'],'after':len(ts),'meshes':stats},indent=2));print('SIMPLIFY',m['static_triangles'],len(ts))
