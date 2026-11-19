"""Install the tested module for NEXT launch, without triggering IDE hot reload.
UE HotReload.cpp watches additions named <current module basename>-*.dll.
This intentionally uses a different basename and changes only the launch manifest.
"""
import hashlib,json,pathlib,shutil
root=pathlib.Path(__file__).resolve().parents[1];docs=root/'Docs/Demo20261119'
summary=json.loads((docs/'acceptance_summary.json').read_text())
assert summary['complete'],'acceptance tests have not passed'
stage=root/'Intermediate/Demo20261119/EditorHost/Binaries/Win64'
source=stage/'UnrealEditor-VirtualGameTeamProj.dll';sha=hashlib.sha256(source.read_bytes()).hexdigest()
runs=json.loads((docs/'acceptance_runs.json').read_text());assert all(x['module_sha256']==sha for x in runs)
bin=root/'Binaries/Win64';manifest=bin/'UnrealEditor.modules'
old=json.loads(manifest.read_text(encoding='utf-8-sig'));built=json.loads((stage/'UnrealEditor.modules').read_text(encoding='utf-8-sig'))
assert old['BuildId']==built['BuildId'],'engine BuildId mismatch'
backup=docs/'original_module_manifest.json'
if not backup.exists():backup.write_text(json.dumps(old,indent=2))
name='Demo20261119_VirtualGameTeamProj_'+sha[:10]+'.dll';target=bin/name
if not target.exists():shutil.copy2(source,target)
assert hashlib.sha256(target.read_bytes()).hexdigest()==sha
old['Modules']['VirtualGameTeamProj']=name
manifest.write_text(json.dumps(old,indent=2))
(docs/'installation.json').write_text(json.dumps(dict(module=str(target),sha256=sha,manifest=str(manifest),takes_effect='next editor/game launch; already running editor is not hot reloaded'),indent=2))
print('Installed for next launch:',target)
