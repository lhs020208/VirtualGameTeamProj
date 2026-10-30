"""Bounded analytic WH cone experiment. End rims/outer shell/other meshes unchanged."""
from pathlib import Path
import json,struct,numpy as np
root=Path(__file__).resolve().parents[1];src=root/'SourceAssets/ProcessPlantPhysics';dst=root/'Docs/ProcessPlantPhysics/optimization3000/analytic';dst.mkdir(exist_ok=True)
f=(src/'runtime.plant').read_bytes();d=np.frombuffer(f,dtype='<f4',offset=4).reshape(-1,4);packed=d.tolist();triangles=[];stack=[int(d[0,0])];removed=[]
# Identify only the unbroken WH inner conical surface by all three vertices.
while stack:
 at=stack.pop();lo,hi=d[at:at+2]
 if lo[3]>=0:stack.extend([int(lo[3]),int(hi[3])]);continue
 for j in range(int(-lo[3])):
  t=d[int(hi[3])+3*j:int(hi[3])+3*j+3,:3].astype(float);z=t[:,2];rad=np.linalg.norm(t[:,:2],axis=1);expected=.022+(z-.7)*(.328/.25)
  cone=np.all((z>=.7-1e-7)&(z<=.95+1e-7)&(abs(rad-expected)<2e-7)) and np.ptp(z)>.2
  if cone:removed.append(t.tolist())
  else:triangles.append(t.tolist())
assert len(removed)==1024,len(removed)
def is_cone(t):
 z=t[:,2];rad=np.linalg.norm(t[:,:2],axis=1);expected=.022+(z-.7)*(.328/.25)
 return np.all((z>=.7-1e-7)&(z<=.95+1e-7)&(abs(rad-expected)<2e-7)) and np.ptp(z)>.2
def clone(old):
 lo,hi=d[old:old+2]
 if lo[3]>=0:
  left=clone(int(lo[3]));right=clone(int(hi[3]))
  if left<0:return right
  if right<0:return left
  at=len(packed);packed.extend([list(lo[:3])+[left],list(hi[:3])+[right]]);return at
 ts=[]
 for j in range(int(-lo[3])):
  t=d[int(hi[3])+3*j:int(hi[3])+3*j+3,:3].astype(float)
  if not is_cone(t):ts.append(t)
 if not ts:return -1
 at=len(packed);start=at+2;packed.extend([list(lo[:3])+[-len(ts)],list(hi[:3])+[start]]);packed.extend([list(v)+[0.] for t in ts for v in t]);return at
newroot=clone(int(d[0,0]))
# Root0 remains the fallback. Root14 is used only 10mm away from either rim,
# and on the interior side within 10mm of the cone. No port or step is removed.
packed[12]=[0,0,.7,.95];packed[13]=[.022,.35,.71,.94];packed[14]=[newroot,1,0,0]
(dst/'runtime.plant').write_bytes(struct.pack('<I',len(packed))+np.asarray(packed,dtype='<f4').tobytes())
for name in ['runtime.routes','test_charge.csv']:(dst/name).write_bytes((src/name).read_bytes())
(dst/'stats.json').write_text(json.dumps({'removed_inner_triangles':len(removed),'retained_static_triangles':len(triangles),'source_cone_sides':512,'max_circle_polygon_radial_error_m':.35*(1-np.cos(np.pi/512)),'analytic_z_range_m':[.71,.94],'fallback':'original static root outside eligible interior shell or after projection exits region','original_file_unchanged':True},indent=2))
print('ANALYTIC',len(removed),len(triangles),len(packed))
