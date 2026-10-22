"""Run complete rendered cycles sequentially. Refuses existing result folders."""
from pathlib import Path
import argparse,subprocess,time,json,hashlib,sys
ROOT=Path(__file__).resolve().parents[1]
EXE=Path('C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe')
def main():
 p=argparse.ArgumentParser();p.add_argument('--start-repeat',type=int,default=0);p.add_argument('--repeats',type=int,default=3);a=p.parse_args()
 out=ROOT/'Docs/DEM/optimization29_36';out.mkdir(exist_ok=True)
 runs=[]
 for rep in range(a.start_repeat,a.start_repeat+a.repeats):
  for kg in [10,30,50]:
   folder=ROOT/f'Saved/Optimization36_{kg}_r{rep}'
   if folder.exists():raise RuntimeError('output exists: '+str(folder))
   args=[str(EXE),str(ROOT/'VirtualGameTeamProj.uproject'),'/Game/L_ProcessTwin','-game','-windowed','-ResX=1280','-ResY=720','-nosound','-unattended','-ProcessBenchmark',f'-ProcessKg={kg}',f'-ProcessRun={rep}','-ddc=InstalledNoZenLocalFallback',f'-abslog={ROOT}/Saved/Optimization36_{kg}_r{rep}.log']
   start=time.monotonic();proc=subprocess.Popen(args,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
   while not (folder/'result.json').exists():
    if time.monotonic()-start>210:raise RuntimeError('rendered cycle timeout: '+str(folder))
    time.sleep(.5)
   result=json.loads((folder/'result.json').read_text(encoding='utf-8-sig'))
   assert result['complete'] and result['rate']>=.1 and result['mass_error_kg']<1e-8 and result['frames']>100 and result['trace_rows']>20
   assert result['mixer_kg']>=kg*.99 and not result['physical_accuracy_validated']
   result.update({'repeat':rep,'result_folder':str(folder),'launch_to_result_s':time.monotonic()-start})
   runs.append(result);(out/'render_progress.json').write_text(json.dumps(runs,indent=2),encoding='utf-8');print(json.dumps(result),flush=True)
   time.sleep(3)
 (out/'render_benchmark.json').write_text(json.dumps({'runs':runs,'source_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ROOT/'Physics/PowderCore/include/powder/bulk_process.hpp',ROOT/'Source/VirtualGameTeamProj/PowderProcessTwin.cpp']},'virtual_scenario_speed_passed':True,'physical_accuracy_validated':False},indent=2),encoding='utf-8')
if __name__=='__main__':main()
