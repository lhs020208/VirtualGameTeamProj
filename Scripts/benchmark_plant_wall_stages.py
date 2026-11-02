from pathlib import Path
import subprocess,json,statistics,numpy as np
root=Path(__file__).resolve().parents[1];out=root/'Docs/ProcessPlantPhysics/hybrid/wall_stages';exe=root/'Intermediate/PowderCore/Release/plant_bench.exe';old=root/'Intermediate/HybridWallStages/baseline_bench.exe';summary={}
for state,checkpoint in [('initial','initial'),('later',str(root/'Docs/ProcessPlantPhysics/hybrid/profile/evolve.bin'))]:
 rates={};deltas={}
 for trial in range(3):
  for label,data,mode in [('baseline','baseline',0),('cached','indexed',2),('shared','indexed',3),('analytic','analytic',3)]:
   if label in ('cached','analytic') and trial>0:continue
   prefix=out/f'final_{state}_{label}_{trial}'
   subprocess.run([str(old if label=='baseline' else exe),str(root/'Intermediate/HybridWallStages'/data),checkpoint,'0','0','3000',str(prefix),'0','1','0','.00001','16','128','0',str(mode)],cwd=root,check=True)
   rates.setdefault(label,[]).append(json.loads(prefix.with_suffix('.json').read_text())['rate'])
   if label!='baseline':
    a=np.fromfile(out/f'final_{state}_baseline_{trial}.bin',dtype='<f4',offset=4);b=np.fromfile(prefix.with_suffix('.bin'),dtype='<f4',offset=4);deltas.setdefault(label,[]).append(float(np.max(abs(a-b))))
 summary[state]={'rates':rates,'median_rates':{k:statistics.median(v) for k,v in rates.items()},'state_max_component_delta':{k:max(v) for k,v in deltas.items()}}
 (out/'final_summary.json').write_text(json.dumps(summary,indent=2))
print(json.dumps(summary,indent=2))
