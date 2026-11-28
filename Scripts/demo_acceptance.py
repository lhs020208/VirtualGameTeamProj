"""Serial two-minute complete-cycle rendered trials. Never overlap GPU trials."""
import pathlib,subprocess,sys,json,hashlib
root=pathlib.Path(__file__).resolve().parents[1];out=root/'Docs/Demo20261119'
module=root/'Intermediate/Demo20261119/EditorHost/Binaries/Win64/UnrealEditor-VirtualGameTeamProj.dll'
records=[]
for i in range(3):
    label=f'demo_final_r{i}'
    code=subprocess.call([sys.executable,str(root/'Scripts/demo_unreal.py'),'test',label,'120'],cwd=root)
    if code:raise SystemExit(code)
    result=json.loads((out/(label+'.json')).read_text(encoding='utf-8-sig'))
    if not result['performance_pass']:raise SystemExit('performance gate failed: '+label)
    result['module_sha256']=hashlib.sha256(module.read_bytes()).hexdigest()
    records.append(result)
    (out/'acceptance_runs.json').write_text(json.dumps(records,indent=2))
    print('FINISHED_TRIAL',i,'FPS',result['fps'],'rate',result['rate'],flush=True)
