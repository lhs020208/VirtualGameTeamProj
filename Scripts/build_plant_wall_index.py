"""Append a spatial index over ORIGINAL collision leaves, preserving leaf order."""
from pathlib import Path
import argparse,json,struct,shutil
import numpy as np

parser=argparse.ArgumentParser()
parser.add_argument('source',type=Path)
parser.add_argument('output',type=Path)
args=parser.parse_args()
assert args.source.resolve()!=args.output.resolve(),'Keep source as reference'
args.output.mkdir(parents=True,exist_ok=True)
for source in args.source.iterdir():
    if source.is_file():shutil.copy2(source,args.output/source.name)
raw=np.frombuffer((args.source/'runtime.plant').read_bytes(),dtype='<f4',offset=4).reshape(-1,4)
assert raw[10,0]==2 and raw[13,2]==0 and raw[13,3]==0
packed=raw.tolist();stats=[]
for tree in range(2):
    root,end=int(raw[12,tree]),int(raw[13,tree])
    leaves=[at for at in range(root,end,2) if raw[at,3]<0]
    lo=raw[leaves,:3];hi=raw[np.array(leaves)+1,:3];centres=(lo+hi)/2
    header=len(packed);packed.append([0,0,len(leaves),1]);pending=[]
    def node(ids):
        at=len(packed);packed.extend([[0.]*4 for _ in range(3)])
        lower=lo[ids].min(0);upper=hi[ids].max(0)
        packed[at+2]=[int(ids.min()),int(ids.max()),0,0]
        if len(ids)<=4:
            packed[at]=[*lower,-len(ids)];packed[at+1]=[*upper,0];pending.append((at,ids))
        else:
            axis=int(np.argmax(np.ptp(centres[ids],axis=0)))
            ids=ids[np.argsort(centres[ids,axis],kind='stable')];mid=len(ids)//2
            first=node(ids[:mid]);node(ids[mid:])
            packed[at]=[*lower,first];packed[at+1]=[*upper,len(packed)]
        return at
    start=node(np.arange(len(leaves)));finish=len(packed)
    for at,ids in pending:
        packed[at+1][3]=len(packed)
        packed.extend([[leaves[i],int(i),0,0] for i in ids])
    packed[header][:2]=[start,finish];packed[13][2+tree]=header
    stats.append(dict(original_leaves=len(leaves),spatial_nodes=(finish-start)//3))
assert len(packed)<4000000
(args.output/'runtime.plant').write_bytes(struct.pack('<I',len(packed))+np.asarray(packed,dtype='<f4').tobytes())
m=json.loads((args.output/'manifest.json').read_text());m['ordered_leaf_index']=stats
(args.output/'manifest.json').write_text(json.dumps(m,indent=2))
print(json.dumps(stats))
