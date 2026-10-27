"""Sequential 50k candidate-list comparison. Close the editor before --run."""
from pathlib import Path
import sys
sys.dont_write_bytecode=True
import argparse,csv,json,math,subprocess,statistics,datetime,shutil
import numpy as np
from scipy.spatial import cKDTree
from run_additional_optimization_baseline import ROOT,BASE,sha,differences,source_inventory,audit

from run_additional_optimization02 import inspect

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--run',action='store_true');parser.add_argument('--repeats',type=int,default=2);parser.add_argument('--seconds',type=int,default=12);args=parser.parse_args()
 if not 1<=args.repeats<=10 or args.seconds<1:parser.error('Invalid run size')
 manifest=json.loads((BASE/'baseline.json').read_text());assert not differences(manifest['protected_sha256']), 'Frozen assets changed'
 assert sha(BASE/manifest['initial_file'])==manifest['initial_sha256']
 command=['C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',str(ROOT/'VirtualGameTeamProj.uproject'),'/Game/L_MX301_Test','-game','-windowed','-ResX=1280','-ResY=720','-nosound','-unattended','-ddc=InstalledNoZenLocalFallback','-ExecCmds=t.MaxFPS 30','-GrainBenchmark','-GrainCount=50000','-GrainDt=0.000375','-GrainIterations=16','-GrainBatch=32',f'-GrainSeconds={args.seconds}']
 if not args.run:print(command);return
 active=subprocess.check_output(['powershell','-NoProfile','-Command','@(Get-Process UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id) | ConvertTo-Json -Compress'],text=True).strip()
 if active and json.loads(active):raise RuntimeError('Close existing Unreal processes: '+active)
 root=ROOT/'Saved'/('AdditionalOpt03_'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ'));root.mkdir()
 report=dict(source_sha256=source_inventory(),binary_sha256={n:sha(ROOT/n) for n in manifest['binary_sha256']},seconds=args.seconds,repeats=args.repeats,command=command,physical_accuracy_validated=False,runs=[])
 # Reverse the second round to reduce fixed-order temperature bias.
 for repeat in range(args.repeats):
  for mode in (['off','candidates'] if repeat%2==0 else ['candidates','off']):
   out=root/f'{mode}_{repeat+1}';out.mkdir();cmd=command+[f'-GrainOutput={out}',f'-abslog={out}/engine.log']+({'off':[],'candidates':['-GrainCandidates','-GrainCandidateCapacity=64']}[mode]);print('START',out,flush=True)
   (out/'settings.json').write_text(json.dumps(dict(command=cmd,mode=mode),indent=2))
   proc=subprocess.Popen(cmd,creationflags=subprocess.CREATE_NO_WINDOW)
   try:code=proc.wait(timeout=600)
   except subprocess.TimeoutExpired:proc.terminate();proc.wait(timeout=30);raise RuntimeError('Own benchmark timed out')
   if code:raise RuntimeError(f'Unreal exit {code}')
   result=inspect(out,'off',manifest,args.seconds);assert result['timing']['candidates_enabled']==(mode=='candidates');
   if mode=='candidates':assert result['timing']['candidate_builds']==int(round(result['worker_timing']['solver_time_s']/float(np.float32(.000375))))*16 and result['timing']['candidate_capacity']==64
   else:assert result['timing']['candidate_builds']==0
   result.update(mode=mode,repeat=repeat+1,folder=str(out));report['runs'].append(result)
   (root/'summary.json').write_text(json.dumps(report,indent=2));print('PASS',mode,result['rate'],flush=True)
 rates={mode:statistics.median(x['rate'] for x in report['runs'] if x['mode']==mode) for mode in ['off','candidates']}
 report['median_rate']=rates;report['relative_elapsed_overhead']={mode:rates['off']/rates[mode]-1 for mode in ['candidates']}
 report['limitation']='Two 12-second repetitions by default. Candidates rebuild every projection iteration; reuse belongs to task 04. Final geometry checks do not certify all intermediate states or real powder accuracy.'
 (root/'summary.json').write_text(json.dumps(report,indent=2))
 dest=ROOT/'Docs/DEM/additional_optimization03';dest.mkdir(exist_ok=True)
 shutil.copy2(root/'summary.json',dest/'measurement.json')
 for run in report['runs']:
  source=Path(run['folder']);target=dest/source.name;target.mkdir(exist_ok=True)
  for name in ['settings.json','trace.csv','result.json','timing.json','worker_timing.json','display_timing.json','final.bin']:shutil.copy2(source/name,target/name)
 print('COMPLETE',root,flush=True)
if __name__=='__main__':main()
