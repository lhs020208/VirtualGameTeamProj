"""Check the frozen baseline by default; --run explicitly launches sequential benchmarks."""
from pathlib import Path
import argparse,hashlib,json,subprocess,sys,struct,math,csv,datetime
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'Docs/DEM/additional_optimization01'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def source_inventory():
 return {p.relative_to(ROOT).as_posix():sha(p) for folder in ['Source','Physics/PowderCore/src','Physics/PowderCore/include','Physics/PowderCore/third_party','Plugins/DEMPrototype/Source'] for p in (ROOT/folder).rglob('*') if p.is_file()}
def differences(files):return [name for name,digest in files.items() if not (ROOT/name).is_file() or sha(ROOT/name)!=digest]
def audit(folder,manifest,seconds):
 result=json.loads((folder/'result.json').read_text(encoding='utf-8-sig'))
 if result.get('error') or result.get('time',0)<seconds:raise RuntimeError('Incomplete/failed run: '+str(result))
 if sha(folder/'initial.bin')!=manifest['initial_sha256']:raise RuntimeError('Initial positions/groups changed')
 initial=list(struct.iter_unpack('<ffff',(BASE/manifest['initial_file']).read_bytes()))
 data=(folder/'final.bin').read_bytes()
 if len(data)!=len(initial)*16:raise RuntimeError('Particle count changed')
 for i,p in enumerate(struct.iter_unpack('<ffff',data)):
  if not all(math.isfinite(v) for v in p) or p[3]!=initial[i][3]:raise RuntimeError('Nonfinite state or group change')
 rows=list(csv.DictReader((folder/'trace.csv').open()))
 if not rows:raise RuntimeError('Missing trace')
 previous=-1
 for row in rows:
  if not all(math.isfinite(float(v)) for v in row.values()):raise RuntimeError('Nonfinite trace')
  t=float(row['time_s'])
  if t<=previous or int(row['total_count'])!=50000 or abs(float(row['mass_kg'])-manifest['benchmark']['mass_kg'])>1e-6 or int(row['gate'])!=0:raise RuntimeError('Trace contract mismatch')
  previous=t
 result['basic_invariants_pass']=True
 result['collision_audit']='Not performed by this runner; detailed contact/geometry checks required before adoption.'
 return result

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--run',action='store_true')
 parser.add_argument('--candidate',action='store_true',help='Allow recorded code/binary changes; frozen scene/config must still match')
 parser.add_argument('--seconds',type=int,choices=[12,30],default=12)
 parser.add_argument('--repeats',type=int,default=3)
 parser.add_argument('--engine',default='C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe')
 parser.add_argument('--report',type=Path)
 args=parser.parse_args()
 if not 1<=args.repeats<=10:parser.error('repeats must be 1..10')
 m=json.loads((BASE/'baseline.json').read_text())
 for file,key in [('saved_scene.json','scene_sha256'),(m['initial_file'],'initial_sha256')]:
  if sha(BASE/file)!=m[key]:raise RuntimeError('Frozen evidence changed: '+file)
 inventory=source_inventory()
 protected=differences(m['protected_sha256']);source=sorted(set(differences(m['source_sha256']))|set(inventory).difference(m['source_sha256']));binaries=differences(m['binary_sha256'])
 if protected:raise RuntimeError('Fixed assets/config changed: '+str(protected))
 if (source or binaries) and not args.candidate:raise RuntimeError('Baseline code/build changed; use --candidate only for intentional comparisons: '+str(source+binaries))
 if not Path(args.engine).is_file():raise RuntimeError('Unreal executable missing')
 b=m['benchmark'];folder=ROOT/'Saved'/('AdditionalOpt_'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ'))
 command=[args.engine,str(ROOT/'VirtualGameTeamProj.uproject'),b['level'],'-game','-windowed','-ResX=1280','-ResY=720','-nosound','-unattended','-ddc=InstalledNoZenLocalFallback','-ExecCmds=t.MaxFPS 30','-GrainBenchmark','-GrainCount=50000','-GrainDt=0.000375','-GrainIterations=16','-GrainBatch=32',f'-GrainSeconds={args.seconds}']
 report=dict(check_pass=True,baseline_commit=m['commit'],candidate=args.candidate,source_changes=source,binary_changes=binaries,seconds=args.seconds,repeats=args.repeats,command=command,new_measurement=False)
 if args.run:
  active=subprocess.check_output(['powershell','-NoProfile','-Command','@(Get-Process UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id) | ConvertTo-Json -Compress'],text=True).strip()
  if active and json.loads(active):raise RuntimeError('Close existing Unreal processes before isolated benchmarks; no processes were stopped: '+active)
  folder.mkdir(exist_ok=False);report['new_measurement']=True;report['runs']=[]
  report['source_sha256']=inventory
  report['binary_sha256']={name:sha(ROOT/name) for name in m['binary_sha256']}
  for i in range(args.repeats):
   out=folder/f'run_{i+1}';out.mkdir()
   (out/'settings.json').write_text(json.dumps(report,indent=2))
   p=subprocess.Popen(command+[f'-GrainOutput={out}',f'-abslog={out}/engine.log'],creationflags=subprocess.CREATE_NO_WINDOW)
   try:code=p.wait(timeout=600)
   except subprocess.TimeoutExpired:
    p.terminate();p.wait(timeout=30);raise RuntimeError('Own benchmark process stopped after timeout')
   if code:raise RuntimeError(f'Unreal failed with exit {code}')
   report['runs'].append(audit(out,m,args.seconds))
   (folder/'summary.json').write_text(json.dumps(report,indent=2))
  print('Results:',folder)
 if args.report:
  args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,indent=2))
 print(json.dumps(report,indent=2))
if __name__=='__main__':main()
