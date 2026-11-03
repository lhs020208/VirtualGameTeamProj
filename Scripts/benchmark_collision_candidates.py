"""Sequential paired benchmark, production data unchanged. Stops on thermal guard."""
from pathlib import Path
import subprocess,json,statistics
import numpy as np
r=Path(__file__).resolve().parents[1];out=r/'Docs/ProcessPlantPhysics/CollisionCandidates';temp=r/'Intermediate/CollisionCandidates'
exe=r/'Intermediate/PowderCore/Release/plant_candidate_bench.exe';old=temp/'reference/plant_candidate_bench.exe'
p=np.array([[2.4+.04*i,0,.578, i%2] for i in range(30)],dtype='<f4');v=np.zeros_like(p)
(temp/'mixer30.bin').write_bytes(np.uint32(30).tobytes()+p.tobytes()+v.tobytes())
cases=[('initial3000',3000,'initial',1000,0),('stall3000',3000,str(temp/'stall_3000.bin'),1000,0),('initial20000',20000,'initial',1000,0),('stall20000',20000,str(temp/'stall_20000.bin'),1000,0),('pipe30',30,str(temp/'stall_30.bin'),3000,0),('mixer30',30,str(temp/'mixer30.bin'),3000,15)]
summary={};temperatures=[]
def run(case,n,checkpoint,steps,rpm,mode,trial,profile=0):
    telemetry=subprocess.check_output(['nvidia-smi','--query-gpu=temperature.gpu,memory.used,utilization.gpu','--format=csv,noheader,nounits'],text=True).strip();temperatures.append(telemetry)
    if int(telemetry.split(',')[0])>=78:raise RuntimeError('GPU temperature guard; benchmark stopped')
    prefix=out/f'{case}_m{mode}_{trial}';cmd=[str(old if mode==3 else exe),str(r/'SourceAssets/ProcessPlantHybrid'),str(n),checkpoint,str(mode),str(steps),str(profile),str(prefix),str(rpm)]
    subprocess.run(cmd,cwd=r,check=True,timeout=240)
    return prefix,json.loads(prefix.with_suffix('.json').read_text())
for case,n,checkpoint,steps,rpm in cases:
    rates={m:[] for m in [3,5,6]};deltas={5:[],6:[]}
    for trial in range(3):
        states={}
        for mode in ([3,5,6] if trial%2==0 else [6,5,3]):
            prefix,result=run(case,n,checkpoint,steps,rpm,mode,trial);rates[mode].append(result['rate']);states[mode]=np.fromfile(prefix.with_suffix('.bin'),dtype='<f4',offset=4)
        for mode in [5,6]:
            delta=float(np.max(abs(states[mode]-states[3])));deltas[mode].append(delta)
            if not np.isfinite(delta) or delta!=0:raise RuntimeError(f'{case} mode{mode} differs: {delta}')
    summary[case]={'count':n,'steps':steps,'physical_s':steps*1e-5,'rpm':rpm,'rates':rates,'median_rates':{m:statistics.median(v) for m,v in rates.items()},'max_component_delta':deltas}
    (out/'paired_summary.json').write_text(json.dumps(summary,indent=2));(out/'gpu_telemetry.json').write_text(json.dumps(temperatures,indent=2))
for mode in [3,5,6]:run('split3000',3000,str(temp/'stall_3000.bin'),500,0,mode,0,2)
print(json.dumps(summary,indent=2))
