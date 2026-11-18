"""Independent FP64 reductions and spatial contact diagnostics of saved states."""
from cuda_measure import ROOT, REPORT, read_state
import json
import math
import sys
import numpy as np
from scipy.spatial import cKDTree

folder=REPORT/(sys.argv[1] if len(sys.argv)>1 else 'benchmark_graph')
radius=.002
mass=4/3*math.pi*radius**3*1200
records=[]
for scene in ('initial','transfer','mixing'):
    for stage in range(3):
        for repeat in range(3):
            stem=f'{scene}_r{repeat}_s{stage}'
            a=read_state(folder/(stem+'.bin'))
            p=a[0,:,:3];v=a[1,:,:3]
            pairs=cKDTree(p).query_pairs(2*radius,output_type='ndarray')
            overlap=2*radius-np.linalg.norm(p[pairs[:,0]]-p[pairs[:,1]],axis=1)
            wh=(np.hypot(p[:,0],p[:,1])<.355)&(p[:,2]>.603)&(p[:,2]<1.78)
            rh=(np.hypot(p[:,0]-3,p[:,1])<.255)&(p[:,2]>2.163)&(p[:,2]<3.7)
            record=dict(scene=scene,stage=stage,repeat=repeat,
                        mass_kg=mass*len(p),species=np.bincount(a[0,:,3].astype(int)).tolist(),
                        centroid_m=p.mean(0).tolist(),kinetic_energy_j=float(.5*mass*np.square(v).sum()),
                        momentum_kg_m_s=(mass*v.sum(0)).tolist(),
                        z_quantiles_m=np.quantile(p[:,2],[0,.1,.5,.9,1]).tolist(),
                        mean_speed_m_s=float(np.linalg.norm(v,axis=1).mean()),
                        contacts=int(len(pairs)),maximum_pair_overlap_m=float(overlap.max(initial=0)),
                        rms_pair_overlap_m=float(np.sqrt(np.mean(overlap**2))) if len(overlap) else 0,
                        weighing_count=int(wh.sum()),receiver_count=int(rh.sum()))
            records.append(record)
summary=[]
for scene in ('initial','transfer','mixing'):
    row=dict(scene=scene,stages=[])
    for stage in range(3):
        selected=[r for r in records if r['scene']==scene and r['stage']==stage]
        metrics={}
        for name in ['kinetic_energy_j','mean_speed_m_s','maximum_pair_overlap_m','rms_pair_overlap_m','weighing_count','receiver_count','contacts']:
            values=[r[name] for r in selected]
            metrics[name]=dict(mean=float(np.mean(values)),min=min(values),max=max(values))
        metrics['centroid_mean_m']=np.mean([r['centroid_m'] for r in selected],axis=0).tolist()
        row['stages'].append(dict(stage=stage,metrics=metrics))
    summary.append(row)
result=dict(records=records,summary=summary,
            note='Diagnostics, not experimental validation. Do not infer trajectory identity from matching aggregate statistics. Nonzero overlap is inherent in the current position-based solver.')
(folder/'state_metrics.json').write_text(json.dumps(result,indent=2))
print(json.dumps(summary,indent=2))
