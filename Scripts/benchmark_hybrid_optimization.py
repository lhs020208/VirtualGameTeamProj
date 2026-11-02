"""Sequential paired runs; never change level or simulation parameters."""
from pathlib import Path
import subprocess,json,statistics
import numpy as np
root=Path(__file__).resolve().parents[1]
out=root/'Docs/ProcessPlantPhysics/hybrid/optimization'
exe=root/'Intermediate/PowderCore/Release/plant_bench.exe'
reference_exe=root/'Intermediate/HybridOptimization/reference_bin/plant_bench.exe'
assert reference_exe.exists(), 'Build the preserved pre-optimization kernel reference executable first'
baseline=root/'Intermediate/HybridOptimization/baseline'
candidate=root/'SourceAssets/ProcessPlantHybrid'
summary={}
for state,checkpoint in [('initial','initial'),('later',str(root/'Docs/ProcessPlantPhysics/hybrid/profile/evolve.bin'))]:
    rates={'baseline':[],'optimized':[]};deltas=[]
    for trial in range(3):
        for label,data in [('baseline',baseline),('optimized',candidate)]:
            prefix=out/f'{state}_{label}_{trial}'
            subprocess.run([str(reference_exe if label=='baseline' else exe),str(data),checkpoint,'0','0','3000',str(prefix),'0','1','0','.00001','16','128','0'],cwd=root,check=True)
            rates[label].append(json.loads(prefix.with_suffix('.json').read_text())['rate'])
        a=np.fromfile(out/f'{state}_baseline_{trial}.bin',dtype='<f4',offset=4)
        b=np.fromfile(out/f'{state}_optimized_{trial}.bin',dtype='<f4',offset=4)
        deltas.append(float(np.max(abs(a-b))))
    summary[state]={'rates':rates,'median_rates':{k:statistics.median(v) for k,v in rates.items()},'max_state_component_difference':max(deltas)}
    for label,data in [('baseline',baseline),('optimized',candidate)]:
        prefix=out/f'{state}_{label}_split'
        subprocess.run([str(reference_exe if label=='baseline' else exe),str(data),checkpoint,'0','1','3000',str(prefix),'0','1','0','.00001','16','128','1'],cwd=root,check=True)
(out/'paired_summary.json').write_text(json.dumps(summary,indent=2))
print(json.dumps(summary,indent=2))
