"""Run the same CPU integration presets in Unreal; no saved settings are changed."""
from pathlib import Path
import argparse,subprocess,time,json,shutil
from audit_coupled_species import audit
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('output',type=Path);p.add_argument('--mode',type=int,choices=range(1,7),required=True);p.add_argument('--render',action='store_true');a=p.parse_args()
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);end=.3 if a.mode in (2,5) else .12
 level='Filter' if a.mode in (2,5) else 'Transport'
 cmd=['C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe',str(ROOT/'VirtualGameTeamProj.uproject'),'/Game/DEM/Tests/L_CoupledPowder_'+level,'-game','-unattended','-nosound','-nop4','-ddc=InstalledNoZenLocalFallback',f'-CoupledExperiment={a.mode}',f'-CoupledSeconds={end}',f'-CoupledOutput={out}',f'-abslog={out}/engine.log','-ExecCmds=t.MaxFPS 30']
 cmd+=['-windowed','-ResX=1280','-ResY=720','-CoupledScreenshot'] if a.render else ['-nullrhi']
 (out/'command.json').write_text(json.dumps(cmd,indent=2));start=time.monotonic();start_wall=time.time();monitor=[]
 with (out/'console.log').open('w') as f:
  proc=subprocess.Popen(cmd,stdout=f,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
  try:
   while proc.poll() is None:
    if time.monotonic()-start>1200:raise RuntimeError('own preset run timeout')
    if a.render:
     q=subprocess.run(['nvidia-smi','--query-gpu=temperature.gpu,power.draw','--format=csv,noheader,nounits'],capture_output=True,text=True,timeout=5,creationflags=subprocess.CREATE_NO_WINDOW)
     v=[float(x) for x in q.stdout.strip().split(',')] if q.returncode==0 else []
     if len(v)!=2 or v[0]>=75 or v[1]>=155:raise RuntimeError('conservative GPU telemetry stop')
     monitor.append(v)
    time.sleep(.5)
   assert proc.returncode==0,proc.returncode
  finally:
   if proc.poll() is None:proc.terminate();proc.wait(timeout=30)
 r=json.loads((out/'result.json').read_text(encoding='utf-8-sig'));m=json.loads((out/'protocol.json').read_text(encoding='utf-8-sig'))
 assert m['contact_kn']==(1000 if a.mode in (2,5) else 100) and m['contact_cn']==(.02 if a.mode in (2,5) else .003),m
 assert not r['failed'] and r['time_s']>=end, (r,(out/'error.txt').read_text(encoding='utf-8-sig') if (out/'error.txt').exists() else '')
 assert abs(r['gas_balance_kg'])<1e-12 and abs(r['particle_balance_kg'])<1e-15
 assert abs(m['supplied_kg']-m['retained_kg']-m['exited_kg'])<1e-15
 for species in m.get('by_type',[]):assert abs(species['supplied_kg']-species['retained_kg']-species['exited_kg'])<1e-15
 if a.mode in (1,6):assert m['exited_kg']>0
 if a.mode==3:assert m['outlet_kg']==0
 if a.mode==4:assert m['exited_kg']==0 and r['count']==6
 if a.mode==2:assert m['exited_kg']==0 and 0<m['filter_contact_kg']<=m['retained_kg'] and r['count']==3
 if a.mode==5:assert m['exited_kg']==0 and m['filter_contact_kg']==0 and m['maximum_filter_contact_kg']>0 and r['count']==3
 species=audit(out)
 screenshot=ROOT/'Saved/CoupledPreview.png'
 if a.render:
  assert screenshot.exists() and screenshot.stat().st_mtime>=start_wall-1;shutil.copy2(screenshot,out/'preview.png')
 summary=dict(passed=True,result=r,protocol=m,species_audit=species,rendered=a.render,monitor=monitor,wall_s=time.monotonic()-start)
 (out/'validation.json').write_text(json.dumps(summary,indent=2));print(json.dumps({k:v for k,v in summary.items() if k!='monitor'},indent=2),flush=True)
if __name__=='__main__':main()
