"""Same-duration, serial renderer trials. Keep failed performance measurements too."""
import pathlib,subprocess,sys,json,hashlib,time
root=pathlib.Path(__file__).resolve().parents[1];out=root/'Docs/DemoScale200k'
records=[]
for label in ['baseline_r0','scaled_r0','visual_r0']:
 command=[sys.executable,str(root/'Scripts/demo_scale_unreal.py'),'test',label,'150','stat unit']
 p=subprocess.Popen(command,cwd=root)
 live=[];last=None
 while p.poll() is None:
  progress=out/(label+'_progress.json')
  if progress.exists():
   try:
    sample=json.loads(progress.read_text(encoding='utf-8-sig'))
    if sample!=last:live.append(sample);last=sample;(out/(label+'_timeline.json')).write_text(json.dumps(live,indent=2))
   except (ValueError,OSError):pass
  time.sleep(1)
 result=json.loads((out/(label+'.json')).read_text(encoding='utf-8-sig'))
 if result['error']:raise RuntimeError(label+' runtime error')
 result['label']=label;result['process_exit']=p.returncode
 result['module_sha256']=hashlib.sha256((root/'Intermediate/Demo20261119/EditorHost/Binaries/Win64/UnrealEditor-VirtualGameTeamProj.dll').read_bytes()).hexdigest()
 records.append(result);(out/'rendered_trials.json').write_text(json.dumps(records,indent=2))
 print('MEASURED',label,result['fps'],result['rate'],flush=True)
