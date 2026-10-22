"""Read-only audit of the extra GPU runs; saves a separate summary."""
import csv, json, math
from pathlib import Path
import numpy as np
from scipy.spatial import cKDTree

root = Path('D:/GitHub/VirtualGameTeamProj/Docs/DEM/optimization_followup')
report = {'dedicated_performance_run': False, 'physical_accuracy_validated': False, 'cases': {}}
for name, count in [('before', 30000), ('after', 30000), ('long50', 50000)]:
    folder = root / name
    rows = list(csv.DictReader((folder/'trace.csv').open()))
    assert rows and all(math.isfinite(float(v)) for row in rows for v in row.values())
    assert all(int(row['count']) == count for row in rows)
    assert float(rows[-1]['time']) >= (30 if name == 'long50' else 4)
    p = np.fromfile(folder/'final.bin', dtype=np.float32).reshape(-1, 4)
    assert len(p) == count and np.isfinite(p).all() and np.isin(p[:,3], [0,1]).all()
    r = (.054/(count*4*math.pi/3))**(1/3)
    main = p[:,2] >= 0
    receiver = p[:,2] < -.25
    bound = main | receiver
    violation = max(float(np.maximum(abs(p[bound,0])-(.45-r),0).max(initial=0)),
                    float(np.maximum(abs(p[bound,1])-(.25-r),0).max(initial=0)),
                    float(np.maximum(p[main,2]-(.4-r),0).max(initial=0)),
                    float(np.maximum(-.6+r-p[receiver,2],0).max(initial=0)))
    pairs = cKDTree(p[:,:3]).query_pairs(2*r, output_type='ndarray')
    overlap = float(np.maximum(2*r-np.linalg.norm(p[pairs[:,0],:3]-p[pairs[:,1],:3],axis=1),0).max(initial=0)/r)
    data = dict(last=rows[-1], max_recorded_overlap_radius=max(float(row['max_overlap_radius']) for row in rows),
                final_overlap_radius=overlap, final_outer_wall_intrusion_m=violation,
                groups=[int(np.sum(p[:,3]==g)) for g in (0,1)], timing=json.loads((folder/'timing.json').read_text()))
    if name == 'long50':
        a = p[main]; bins = np.clip(((a[:,:3]+[.45,.25,0])/[.9,.5,.4]*4).astype(int),0,3)
        ids=(bins[:,2]*4+bins[:,1])*4+bins[:,0]
        sizes=np.bincount(ids,minlength=64); first=np.bincount(ids,weights=(a[:,3]==0),minlength=64)
        fraction=float(np.mean(a[:,3]==0)) if len(a) else 0
        occupied=sizes>0
        mix=1-float(np.sum(sizes[occupied]/len(a)*(first[occupied]/sizes[occupied]-fraction)**2))/(fraction*(1-fraction)) if 0<fraction<1 else -1
        assert abs(mix-float(rows[-1]['mix_index'])) < 1e-6
        data['independent_mixing_index']=mix
        data['remaining_mixer_count']=int(main.sum())
        data['receiver_mass_kg']=int(receiver.sum())*64.8/count
    assert violation < 1e-4, 'outer boundary intrusion'
    assert overlap < .1, 'final overlap tolerance exceeded'
    report['cases'][name]=data
before=np.fromfile(root/'before/final.bin',dtype=np.float32).reshape(-1,4)
after=np.fromfile(root/'after/final.bin',dtype=np.float32).reshape(-1,4)
assert np.array_equal(before[:,3],after[:,3])
report['comparison']={'persistent_group_identity':True,'mean_height_difference_m':float(abs(before[:,2].mean()-after[:,2].mean())),
    'mix_difference':abs(float(report['cases']['before']['last']['mix_index'])-float(report['cases']['after']['last']['mix_index']))}
(root/'audit.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
