"""Dense fixture diagnostics, including repeatability of the frozen baseline."""
from cuda_measure import *
import sys

folder = REPORT/(sys.argv[1] if len(sys.argv)>1 else 'short_contact_ptx')
folder.mkdir(exist_ok=True)
scenes = json.loads((REPORT/'fixtures.json').read_text())
records = []
for scene in scenes:
    for steps in (1, 16):
        for s, r in ((0, 0), (0, 1), (1, 0), (2, 0)):
            out = folder/f"{scene['name']}_n{steps}_s{s}_r{r}"
            if not out.with_suffix('.json').exists():
                run(s, scene, steps, out)
        def state(stage, repeat=0):
            return read_state(folder/f"{scene['name']}_n{steps}_s{stage}_r{repeat}.bin")
        for label, a, b in [('OpenCL repeat',state(0),state(0,1)),
                             ('CUDA vs OpenCL',state(0),state(1)),
                             ('graph vs CUDA',state(1),state(2))]:
            delta=np.linalg.norm(a[:,:,:3]-b[:,:,:3],axis=2)
            record=dict(scene=scene['name'], steps=steps, comparison=label,
                        position_max_m=float(delta[0].max()),velocity_max_m_s=float(delta[1].max()),
                        position_rms_m=float(np.sqrt(np.mean(delta[0]**2))),
                        velocity_rms_m_s=float(np.sqrt(np.mean(delta[1]**2))),
                        position_changed=int((delta[0]!=0).sum()),velocity_changed=int((delta[1]!=0).sum()))
            records.append(record)
            print(record,flush=True)
(folder/'comparison.json').write_text(json.dumps(records,indent=2))
