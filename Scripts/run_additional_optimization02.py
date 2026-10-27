"""Sequential 50k profiling comparison. Close the editor before --run."""
from pathlib import Path
import sys
sys.dont_write_bytecode=True
import argparse,csv,json,math,subprocess,statistics,datetime,shutil
import numpy as np
from scipy.spatial import cKDTree
from run_additional_optimization_baseline import ROOT,BASE,sha,differences,source_inventory,audit

def geometry_audit(folder):
 p=np.fromfile(folder/'final.bin',dtype='<f4').reshape(-1,4)[:,:3].astype(float)
 r=float(np.float32((.054/(50000*4*math.pi/3))**(1/3)))
 last=list(csv.DictReader((folder/'trace.csv').open()))[-1];angle=float(last['angle_rad'])
 pairs=cKDTree(p).query_pairs(2*r,output_type='ndarray')
 overlap=float(np.maximum(2*r-np.linalg.norm(p[pairs[:,0]]-p[pairs[:,1]],axis=1),0).max(initial=0)/r)
 geometry=json.loads((ROOT/'SourceAssets/Granular/MX301_convex.json').read_text())
 wall=0.;rotor=0.
 for plane in geometry['interior'][0]['planes']:
  a=np.array(plane);wall=max(wall,float(np.maximum(p@a[:3]-a[3]+r,0).max(initial=0)))
 local=p.copy();local[:,1]=np.cos(angle)*p[:,1]+np.sin(angle)*p[:,2];local[:,2]=-np.sin(angle)*p[:,1]+np.cos(angle)*p[:,2]
 for part in geometry['rotor']:
  bounds=np.array(part['bounds']);v=local[((local>=bounds[:,0]-r)&(local<=bounds[:,1]+r)).all(axis=1)]
  if not len(v):continue
  d=np.full(len(v),-np.inf)
  for a in np.array(part['planes']):d=np.maximum(d,v@a[:3]-a[3])
  rotor=max(rotor,float(np.maximum(r-d,0).max(initial=0)))
 if overlap>.1 or wall>.0001 or rotor>.0001:raise RuntimeError(f'Geometry limits exceeded: {overlap}, {wall}, {rotor}')
 return dict(snapshot_time_s=float(last['time_s']),overlap_radius=overlap,wall_penetration_m=wall,rotor_penetration_m=rotor,mixing_index=float(last['mix_index']),mean_z_m=float(p[:,2].mean()),scope='final snapshot only')

def inspect(out,mode,manifest,seconds):
 result=audit(out,manifest,seconds);result['geometry']=geometry_audit(out)
 result.pop('collision_audit',None)
 for name in ['timing','worker_timing','display_timing']:
  result[name]=json.loads((out/(name+'.json')).read_text())
 t=result['timing'];assert t['profiling_enabled']==(mode!='off') and t['split_boundaries']==(mode=='split')
 if mode!='off':
  assert t['grid_clear_calls']==t['grid_build_calls']==16*t['predict_calls']
  assert t['snapshot_readback_gpu_s']>0
  key='particle_contact' if mode=='split' else 'contact_and_boundary'
  assert t[key+'_calls']==t['grid_clear_calls'] and t[key+'_gpu_s']>0
  if mode=='split':assert t['boundary_projection_calls']==t['grid_clear_calls'] and t['boundary_probe_calls']==t['predict_calls']
 assert result['display_timing']['updates']>0
 return result

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--run',action='store_true');parser.add_argument('--repeats',type=int,default=2);parser.add_argument('--seconds',type=int,default=12);args=parser.parse_args()
 if not 1<=args.repeats<=10 or args.seconds<1:parser.error('Invalid run size')
 manifest=json.loads((BASE/'baseline.json').read_text());assert not differences(manifest['protected_sha256']), 'Frozen assets changed'
 assert sha(BASE/manifest['initial_file'])==manifest['initial_sha256']
 command=['C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',str(ROOT/'VirtualGameTeamProj.uproject'),'/Game/L_MX301_Test','-game','-windowed','-ResX=1280','-ResY=720','-nosound','-unattended','-ddc=InstalledNoZenLocalFallback','-ExecCmds=t.MaxFPS 30','-GrainBenchmark','-GrainCount=50000','-GrainDt=0.000375','-GrainIterations=16','-GrainBatch=32',f'-GrainSeconds={args.seconds}']
 if not args.run:print(command);return
 active=subprocess.check_output(['powershell','-NoProfile','-Command','@(Get-Process UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id) | ConvertTo-Json -Compress'],text=True).strip()
 if active and json.loads(active):raise RuntimeError('Close existing Unreal processes: '+active)
 root=ROOT/'Saved'/('AdditionalOpt02_'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ'));root.mkdir()
 report=dict(source_sha256=source_inventory(),binary_sha256={n:sha(ROOT/n) for n in manifest['binary_sha256']},seconds=args.seconds,repeats=args.repeats,command=command,physical_accuracy_validated=False,runs=[])
 # Reverse the second round to reduce fixed-order temperature bias.
 for repeat in range(args.repeats):
  for mode in (['off','on','split'] if repeat%2==0 else ['split','on','off']):
   out=root/f'{mode}_{repeat+1}';out.mkdir();cmd=command+[f'-GrainOutput={out}',f'-abslog={out}/engine.log']+({'off':[],'on':['-GrainProfile'],'split':['-GrainProfileSplit']}[mode]);print('START',out,flush=True)
   (out/'settings.json').write_text(json.dumps(dict(command=cmd,mode=mode),indent=2))
   proc=subprocess.Popen(cmd,creationflags=subprocess.CREATE_NO_WINDOW)
   try:code=proc.wait(timeout=600)
   except subprocess.TimeoutExpired:proc.terminate();proc.wait(timeout=30);raise RuntimeError('Own benchmark timed out')
   if code:raise RuntimeError(f'Unreal exit {code}')
   result=inspect(out,mode,manifest,args.seconds);result.update(mode=mode,repeat=repeat+1,folder=str(out));report['runs'].append(result)
   (root/'summary.json').write_text(json.dumps(report,indent=2));print('PASS',mode,result['rate'],flush=True)
 rates={mode:statistics.median(x['rate'] for x in report['runs'] if x['mode']==mode) for mode in ['off','on','split']}
 report['median_rate']=rates;report['relative_elapsed_overhead']={mode:rates['off']/rates[mode]-1 for mode in ['on','split']}
 report['limitation']='Two short repetitions by default; split timings include extra dispatch/global-memory costs and cannot be added to fused timings. ISM timing is CPU submission, not render GPU time.'
 (root/'summary.json').write_text(json.dumps(report,indent=2))
 dest=ROOT/'Docs/DEM/additional_optimization02';dest.mkdir(exist_ok=True)
 shutil.copy2(root/'summary.json',dest/'measurement.json')
 for run in report['runs']:
  source=Path(run['folder']);target=dest/source.name;target.mkdir(exist_ok=True)
  for name in ['settings.json','trace.csv','result.json','timing.json','worker_timing.json','display_timing.json','final.bin']:shutil.copy2(source/name,target/name)
 print('COMPLETE',root,flush=True)
if __name__=='__main__':main()
