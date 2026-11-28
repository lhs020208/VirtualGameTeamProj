"""Read-only source audit and compact rotor collision proxies for the demo."""
import hashlib, json, pathlib
import numpy as np
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import connected_components
root=pathlib.Path(__file__).resolve().parents[1]
out=root/'Docs/Demo20261119';out.mkdir(parents=True,exist_ok=True)
assets=root/'SourceAssets/ProcessPlant_Demo20261119';assets.mkdir(parents=True,exist_ok=True)
source=root/'SourceAssets/ProcessPlant_20261111_LowPoly/Simulation'
paths=[root/'Content/ProcessPlant_20261111_LowPoly/L_ProcessPlant_LowPoly_1111.umap',root/'Content/ProcessPlant_CUDA_20261118/L_ProcessPlant_CUDA_1118.umap',source/'runtime.plant',source/'runtime.routes',source/'test_charge.csv']
receipt=out/'original_hashes.json'
hashes={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
if receipt.exists(): assert json.loads(receipt.read_text())==hashes,'source changed'
else: receipt.write_text(json.dumps(hashes,indent=2))
d=np.fromfile(source/'runtime.plant',dtype='<f4',offset=4).reshape(-1,4)
tri=[];stack=[int(d[0,1])]
while stack:
    at=stack.pop();lo,hi=d[at:at+2]
    if lo[3]>=0: stack.extend([int(lo[3]),int(hi[3])])
    else:
        for j in range(int(-lo[3])): tri.append(d[int(hi[3])+3*j:int(hi[3])+3*j+3,:3])
tri=np.array(tri,dtype=float)
verts,inv=np.unique(np.round(tri.reshape(-1,3),5),axis=0,return_inverse=True)
ids=inv.reshape(-1,3);a=ids[:,[0,1,2]].ravel();b=ids[:,[1,2,0]].ravel()
n,labels=connected_components(coo_matrix((np.ones(len(a)),(a,b)),shape=(len(verts),len(verts))),directed=False)
boxes=[]
for i in range(n):
    points=verts[labels==i]
    if len(points)<4: continue
    center=points.mean(axis=0);_,axes=np.linalg.eigh(np.cov((points-center).T));axes=axes.T
    local=(points-center)@axes.T;lo=local.min(axis=0);hi=local.max(axis=0)
    center=center+(lo+hi)/2@axes
    boxes.append((center,axes,np.maximum((hi-lo)/2,.0005)))
with (assets/'rotor_boxes.txt').open('w') as f:
    f.write(str(len(boxes))+'\n')
    for c,axes,h in boxes:
        f.write(' '.join(map(str,np.r_[c,axes.ravel(),h]))+'\n')
    f.write(str(float(np.hypot(tri[:,:,1]-d[1,1],tri[:,:,2]-d[1,2]).max()))+'\n')
(out/'geometry.json').write_text(json.dumps(dict(rotor_triangles=len(tri),rotor_components=n,rotor_boxes=len(boxes),source='Original rotor triangle BVH, connected-component PCA boxes; conservative demo proxies'),indent=2))
print('Preserved originals; rotor boxes:',len(boxes))
