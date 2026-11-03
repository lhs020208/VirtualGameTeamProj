"""Separate diagnostic restarts; production particles and assets are untouched."""
from pathlib import Path
import numpy as np,json,hashlib
r=Path(__file__).resolve().parents[1];out=r/'Intermediate/CollisionCandidates';out.mkdir(exist_ok=True,parents=True)
live=np.loadtxt(r/'Docs/ProcessPlantPhysics/BV122Inspection/live_a.csv',delimiter=',',skiprows=1)
for n in [30,3000,20000]:
    q=live[np.arange(n)*len(live)//n];p=np.column_stack((q[:,2:5],q[:,1])).astype('<f4');v=np.zeros_like(p)
    (out/f'stall_{n}.bin').write_bytes(np.uint32(n).tobytes()+p.tobytes()+v.tobytes())
report={'snapshot':'live_a.csv','velocity':'zero: frozen-position restart, not original live velocity','counts':[30,3000,20000],'runtime_sha256':hashlib.sha256((r/'SourceAssets/ProcessPlantHybrid/runtime.plant').read_bytes()).hexdigest(),'reference_exe_sha256':hashlib.sha256((out/'reference/plant_candidate_bench.exe').read_bytes()).hexdigest()}
(r/'Docs/ProcessPlantPhysics/CollisionCandidates/baseline.json').write_text(json.dumps(report,indent=2))
