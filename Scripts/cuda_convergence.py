"""Separate baseline time-step sensitivity study; excluded from speedup claims."""
from cuda_measure import *
folder=REPORT/'convergence'
folder.mkdir(exist_ok=True)
scenes=[dict(name='initial',phase=0,rpm=0)]+json.loads((REPORT/'fixtures.json').read_text())
records=[]
for scene in scenes:
    original=read_state(REPORT/'benchmark_graph'/f"{scene['name']}_r0_s0.bin")
    states=[original]
    for scale in (2,4):
        out=folder/f"{scene['name']}_dt_div_{scale}"
        if not out.with_suffix('.json').exists():
            run(0,scene,3200*scale,out,1e-5/scale)
        states.append(read_state(out.with_suffix('.bin')))
    for i,j in ((0,1),(1,2),(0,2)):
        a,b=states[i],states[j]
        delta=np.linalg.norm(a[:,:,:3]-b[:,:,:3],axis=2)
        record=dict(scene=scene['name'],dt_a=1e-5/(2**i),dt_b=1e-5/(2**j),
                    position_max_m=float(delta[0].max()),position_rms_m=float(np.sqrt(np.mean(delta[0]**2))),
                    velocity_rms_m_s=float(np.sqrt(np.mean(delta[1]**2))),
                    centroid_difference_m=float(np.linalg.norm(a[0,:,:3].mean(0)-b[0,:,:3].mean(0))),
                    mean_speed_a=float(np.linalg.norm(a[1,:,:3],axis=1).mean()),
                    mean_speed_b=float(np.linalg.norm(b[1,:,:3],axis=1).mean()))
        records.append(record)
    (folder/'summary.json').write_text(json.dumps(dict(records=records,
        interpretation='Baseline sensitivity, not experimental validation or proof of convergence. No physical setting in the adopted level is changed by this diagnostic.'),indent=2))
    print(json.dumps(records[-3:],indent=2),flush=True)
