"""Exact-order static leaf candidates per spatial cell; no surface simplification."""
import numpy as np,struct,json,time
from collections import defaultdict
from pathlib import Path
root=Path(__file__).resolve().parents[1];src=root/'SourceAssets/ProcessPlantPhysics';dst=root/'Docs/ProcessPlantPhysics/optimization3000/spatial';dst.mkdir(exist_ok=True)
f=(src/'runtime.plant').read_bytes();d=np.frombuffer(f,dtype='<f4',offset=4).reshape(-1,4);h=.1;r=.00125;origin=d[2,:3].astype(float);dims=np.ceil((d[3,:3]-origin)/h).astype(int);bins=defaultdict(list);stack=[int(d[0,0])];leaves=[]
while stack:
 at=stack.pop();lo,hi=d[at:at+2]
 if lo[3]>=0:stack.extend([int(lo[3]),int(hi[3])]);continue
 leaves.append(at);a=np.maximum(0,np.floor((lo[:3]-r-origin)/h).astype(int));b=np.minimum(dims-1,np.floor((hi[:3]+r-origin)/h).astype(int))
 for z in range(a[2],b[2]+1):
  for y in range(a[1],b[1]+1):
   for x in range(a[0],b[0]+1):bins[(z*dims[1]+y)*dims[0]+x].append(at)
p=d.tolist();start=len(p);p.extend([[0.,0.,0.,0.] for _ in range(int(np.prod(dims)))])
for key,ids in bins.items():p[start+key]=[len(p),len(ids),0,0];p.extend([[i,0,0,0] for i in ids])
p[9]=[start,*map(int,dims)];p[10]=[*origin,h];p[11]=[r,0,0,0];p[12]=[2.25,-.45,.57,0];p[13]=[3.75,.45,1.31,0]
assert len(p)<4000000,len(p)
(dst/'runtime.plant').write_bytes(struct.pack('<I',len(p))+np.asarray(p,dtype='<f4').tobytes())
for name in ['runtime.routes','test_charge.csv']:(dst/name).write_bytes((src/name).read_bytes())
(dst/'stats.json').write_text(json.dumps({'cell_m':h,'radius_m':r,'leaves':len(leaves),'occupied_cells':len(bins),'references':sum(map(len,bins.values())),'extra_bytes':(len(p)-len(d))*16,'max_candidates':max(map(len,bins.values()))},indent=2));print('SPATIAL',len(p),sum(map(len,bins.values())))
