"""Sequential 50k kernel argument cache and transposed candidate layout comparison on top of safe candidate reuse. Close the editor before --run."""
from pathlib import Path
import sys
sys.dont_write_bytecode=True
import argparse,csv,json,math,subprocess,statistics,datetime,shutil
import numpy as np
from scipy.spatial import cKDTree
from run_additional_optimization_baseline import ROOT,BASE,sha,differences,source_inventory,audit

from run_additional_optimization02 import inspect,geometry_audit

def audit_history(out):
 import tempfile
 rows=[];initial=np.fromfile(out/'initial.bin',dtype='<f4').reshape(-1,4)
 for meta in sorted((out/'audit').glob('*.json'),key=lambda p:int(p.stem)):
  data=json.loads(meta.read_text());points=np.fromfile(meta.with_suffix('.bin'),dtype='<f4').reshape(-1,4)
  assert len(points)==50000 and np.isfinite(points).all() and np.array_equal(points[:,3],initial[:,3])
  with tempfile.TemporaryDirectory(dir=out) as temp:
   folder=Path(temp);shutil.copyfile(meta.with_suffix('.bin'),folder/'final.bin')
   with (folder/'trace.csv').open('w',newline='') as f:
    writer=csv.DictWriter(f,fieldnames=data.keys());writer.writeheader();writer.writerow(data)
   rows.append(geometry_audit(folder))
 assert rows, 'Missing history snapshots'
 return rows

def speed_windows(out):
 rows=list(csv.DictReader((out/'trace.csv').open()))
 windows=[]
 for i,a in enumerate(rows):
  if float(a['active_wall_s'])<5:continue
  for b in rows[i+1:]:
   wall=float(b['active_wall_s'])-float(a['active_wall_s'])
   if wall>=5:
    windows.append((float(b['time_s'])-float(a['time_s']))/wall);break
 return dict(window_wall_s_at_least=5,skip_first_wall_s=5,count=len(windows),minimum=min(windows) if windows else None,median=statistics.median(windows) if windows else None,scope='Observed intervals only; not a future guaranteed minimum')

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--run',action='store_true');parser.add_argument('--repeats',type=int,default=2);parser.add_argument('--seconds',type=int,default=12);parser.add_argument('--modes',nargs='+',choices=['kernel','stream','boundary','all','default'],default=['kernel','stream','boundary','all']);parser.add_argument('--label',choices=['comparison','extended60','default60'],default='comparison');args=parser.parse_args()
 if not 1<=args.repeats<=10 or args.seconds<1:parser.error('Invalid run size')
 manifest=json.loads((BASE/'baseline.json').read_text());assert not differences(manifest['protected_sha256']), 'Frozen assets changed'
 assert sha(BASE/manifest['initial_file'])==manifest['initial_sha256']
 command=['C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',str(ROOT/'VirtualGameTeamProj.uproject'),'/Game/L_MX301_Test','-game','-windowed','-ResX=1280','-ResY=720','-nosound','-unattended','-ddc=InstalledNoZenLocalFallback','-ExecCmds=t.MaxFPS 30','-GrainBenchmark','-GrainCount=50000','-GrainDt=0.000375','-GrainIterations=16','-GrainBatch=32',f'-GrainSeconds={args.seconds}']
 if not args.run:print(command);return
 active=subprocess.check_output(['powershell','-NoProfile','-Command','@(Get-Process UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id) | ConvertTo-Json -Compress'],text=True).strip()
 if active and json.loads(active):raise RuntimeError('Close existing Unreal processes: '+active)
 root=ROOT/'Saved'/('AdditionalOpt08_'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ'));root.mkdir()
 report=dict(source_sha256=source_inventory(),binary_sha256={n:sha(ROOT/n) for n in manifest['binary_sha256']},seconds=args.seconds,repeats=args.repeats,command=command,physical_accuracy_validated=False,runs=[])
 # Reverse the second round to reduce fixed-order temperature bias.
 for repeat in range(args.repeats):
  for mode in (args.modes if repeat%2==0 else list(reversed(args.modes))):
   out=root/f'{mode}_{repeat+1}';out.mkdir();cmd=command+[f'-GrainOutput={out}',f'-abslog={out}/engine.log']+({'kernel':['-GrainCandidateReuse','-GrainKernelOptimize'], 'stream':['-GrainCandidateReuse','-GrainKernelOptimize','-GrainStream'], 'boundary':['-GrainCandidateReuse','-GrainKernelOptimize','-GrainBoundaryCandidates'], 'all':['-GrainCandidateReuse','-GrainKernelOptimize','-GrainStream','-GrainBoundaryCandidates'], 'default':[]}[mode])+(['-GrainAuditSnapshots'] if args.seconds>=60 else []);print('START',out,flush=True)
   (out/'settings.json').write_text(json.dumps(dict(command=cmd,mode=mode),indent=2))
   proc=subprocess.Popen(cmd,creationflags=subprocess.CREATE_NO_WINDOW)
   try:code=proc.wait(timeout=600)
   except subprocess.TimeoutExpired:proc.terminate();proc.wait(timeout=30);raise RuntimeError('Own benchmark timed out')
   if code:raise RuntimeError(f'Unreal exit {code}')
   result=inspect(out,'off',manifest,args.seconds);assert result['timing']['candidates_enabled'] is True;
   t=result['timing'];assert t['boundary_candidates_enabled']==(mode in ['boundary','all']);assert result['worker_timing']['stream_enabled']==(mode in ['stream','all']);assert t['kernel_optimize_enabled'];assert t['candidate_reuse_enabled'] and not t['candidate_audit_enabled']
   assert t['reuse_checks']==int(round(result['worker_timing']['solver_time_s']/float(np.float32(.000375))))*16
   assert t['candidate_builds']+t['candidate_reuses']==t['reuse_checks'] and 0<t['candidate_builds']<t['reuse_checks']
   assert t['candidate_capacity']==64 and t['candidate_skin_m']>0
   if args.seconds>=60:result['history_audit']=audit_history(out)
   result['speed_windows']=speed_windows(out)
   result.update(mode=mode,repeat=repeat+1,folder=str(out));report['runs'].append(result)
   (root/'summary.json').write_text(json.dumps(report,indent=2));print('PASS',mode,result['rate'],flush=True)
 rates={mode:statistics.median(x['rate'] for x in report['runs'] if x['mode']==mode) for mode in args.modes}
 report['median_rate']=rates;report['relative_elapsed_overhead']={}
 report['limitation']='Two 12-second repetitions by default. Every contact iteration checks displacement; no physical parameters changed. Final geometry checks do not certify all intermediate states or real powder accuracy.'
 (root/'summary.json').write_text(json.dumps(report,indent=2))
 dest=ROOT/'Docs/DEM/additional_optimization08'/args.label;dest.mkdir(parents=True,exist_ok=True)
 shutil.copy2(root/'summary.json',dest/'measurement.json')
 for run in report['runs']:
  source=Path(run['folder']);target=dest/source.name;target.mkdir(exist_ok=True)
  for name in ['settings.json','trace.csv','result.json','timing.json','worker_timing.json','display_timing.json','final.bin']:shutil.copy2(source/name,target/name)
 print('COMPLETE',root,flush=True)
if __name__=='__main__':main()
