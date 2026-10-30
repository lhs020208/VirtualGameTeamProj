"""Audit an exported plant snapshot; geometry containment is a separate test."""
import argparse,csv,json
from pathlib import Path
import numpy as np
from scipy.spatial import cKDTree
parser=argparse.ArgumentParser();parser.add_argument('snapshot',type=Path);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
with args.snapshot.open() as f: rows=list(csv.reader(f))[1:]
a=np.array(rows,dtype=float);ids=a[:,0];species=a[:,1];p=a[:,2:5];radius=.00125
assert len(a)>0 and np.isfinite(a).all() and np.array_equal(ids,np.arange(len(a)))
assert np.all(species==np.floor(species)) and np.all((species>=0)&(species<32))
pairs=cKDTree(p).query_pairs(2*radius,output_type='ndarray')
overlap=2*radius-np.linalg.norm(p[pairs[:,0]]-p[pairs[:,1]],axis=1) if len(pairs) else np.array([])
report={'count':len(a),'species_counts':{str(int(k)):int(v) for k,v in zip(*np.unique(species,return_counts=True))},'finite':True,'unique_sequential_ids':True,'overlapping_pairs':len(pairs),'max_overlap_diameter_fraction':float(overlap.max()/(2*radius)) if len(overlap) else 0.,'counts':{'weighing':int(np.sum((np.linalg.norm(p[:,:2],axis=1)<.355)&(p[:,2]>.603)&(p[:,2]<1.78))),'receiver':int(np.sum((np.linalg.norm(p[:,:2]-[3,0],axis=1)<.255)&(p[:,2]>2.163)&(p[:,2]<3.7))),'mixer':int(np.sum((p[:,0]>2.25)&(p[:,0]<3.75)&(abs(p[:,1])<.45)&(p[:,2]>.57)&(p[:,2]<1.31)))},'note':'One snapshot only; not a trajectory-wide overlap bound, wall-containment proof or physical calibration.'}
args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report,indent=2))
