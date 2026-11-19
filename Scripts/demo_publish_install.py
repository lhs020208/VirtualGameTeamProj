"""Install only the verified 200k-display / 2500-solver module for the next launch."""
import hashlib,json,pathlib,shutil
root=pathlib.Path(__file__).resolve().parents[1];out=root/'Docs/Demo200kSetup'
read=lambda name:json.loads((out/name).read_text(encoding='utf-8-sig'))
test=read('verify_staged.json')
assert test['configuration_pass'] and not test['error']
assert test['display_particles']==200000 and test['solver_particles']==2500 and .98<=test['rate']<=1.02
assert test['MX']==20000,'full input charge did not reach the mixer'
for label in ['original_opencl','original_cuda']:assert not read(label+'.json')['failed']
stage=root/'Intermediate/Demo20261119/EditorHost/Binaries/Win64';source=stage/'UnrealEditor-VirtualGameTeamProj.dll'
digest=hashlib.sha256(source.read_bytes()).hexdigest()
verified=read('verified_module.json');assert verified['sha256']==digest
manifest=root/'Binaries/Win64/UnrealEditor.modules'
old=json.loads(manifest.read_text(encoding='utf-8-sig'));new=json.loads((stage/'UnrealEditor.modules').read_text(encoding='utf-8-sig'));assert old['BuildId']==new['BuildId']
backup=out/'previous_module_manifest.json'
if not backup.exists():backup.write_text(json.dumps(old,indent=2))
name='Demo200k20261119_VirtualGameTeamProj_'+digest[:10]+'.dll';target=manifest.parent/name
if not target.exists():shutil.copy2(source,target)
assert hashlib.sha256(target.read_bytes()).hexdigest()==digest
old['Modules']['VirtualGameTeamProj']=name;manifest.write_text(json.dumps(old,indent=2))
(out/'installation.json').write_text(json.dumps(dict(module=str(target),sha256=digest,manifest=str(manifest),next_launch_only=True,performance_pass=test['performance_pass'],configuration_pass=True),indent=2))
print('Installed for next launch:',target)
