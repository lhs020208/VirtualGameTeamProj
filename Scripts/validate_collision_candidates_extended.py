"""Longer deterministic comparisons; editor should be idle/closed for both runs."""
from pathlib import Path
import subprocess,json
import numpy as np
r=Path(__file__).resolve().parents[1];out=r/'Docs/ProcessPlantPhysics/CollisionCandidates';temp=r/'Intermediate/CollisionCandidates'
raw=np.fromfile(temp/'stall_30.bin',dtype='<f4',offset=4).reshape(2,30,4);raw[1,:,:3]=[2,0,0]
(temp/'fast30.bin').write_bytes(np.uint32(30).tobytes()+raw.tobytes())
summary={}
for label,n,checkpoint,steps,rpm in [('extended_stall20000',20000,'stall_20000.bin',5000,0),('extended_mixer30',30,'mixer30.bin',20000,15),('fast30',30,'fast30.bin',10000,0)]:
    states={};results={}
    for mode in [3,5]:
        telemetry=subprocess.check_output(['nvidia-smi','--query-gpu=temperature.gpu','--format=csv,noheader,nounits'],text=True)
        if int(telemetry.strip())>=78:raise RuntimeError('GPU temperature guard')
        exe=temp/'reference/plant_candidate_bench.exe' if mode==3 else r/'Intermediate/PowderCore/Release/plant_candidate_bench.exe';prefix=out/f'{label}_m{mode}'
        subprocess.run([str(exe),str(r/'SourceAssets/ProcessPlantHybrid'),str(n),str(temp/checkpoint),str(mode),str(steps),'0',str(prefix),str(rpm)],cwd=r,check=True,timeout=240)
        states[mode]=np.fromfile(prefix.with_suffix('.bin'),dtype='<f4',offset=4);results[mode]=json.loads(prefix.with_suffix('.json').read_text())
    delta=float(np.max(abs(states[3]-states[5])));summary[label]={'max_component_delta':delta,'exact_trajectory_match':delta==0,'results':results}
    (out/'extended_summary.json').write_text(json.dumps(summary,indent=2))
    if not np.isfinite(delta):raise RuntimeError('nonfinite extended regression')
    if delta!=0:
        # Dense particle-pair accumulation is not deterministic in the existing
        # solver. Do not silently accept a tolerance: record an independent
        # baseline repeat and require the same-input GPU wall oracle separately.
        prefix=out/f'{label}_repeat_baseline';exe=temp/'reference/plant_candidate_bench.exe'
        subprocess.run([str(exe),str(r/'SourceAssets/ProcessPlantHybrid'),str(n),str(temp/checkpoint),'3',str(steps),'0',str(prefix),str(rpm)],cwd=r,check=True,timeout=240)
        repeated=np.fromfile(prefix.with_suffix('.bin'),dtype='<f4',offset=4)
        summary[label]['baseline_repeat_max_component_delta']=float(np.max(abs(states[3]-repeated)))
        summary[label]['acceptance']='trajectory equality not established; inspect same-input wall audit'
        (out/'extended_summary.json').write_text(json.dumps(summary,indent=2))
