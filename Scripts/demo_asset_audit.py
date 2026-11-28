"""Read-only /Game dependency audit before removing unused Content folders."""
import unreal,pathlib,json,re,collections
root=pathlib.Path(unreal.Paths.project_dir()).resolve();out=root/'Docs/Demo200kSetup';out.mkdir(parents=True,exist_ok=True)
ar=unreal.AssetRegistryHelpers.get_asset_registry();ar.search_all_assets(True);ar.scan_paths_synchronous(['/Game'],True)
opts=unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True,include_searchable_names=True,include_soft_management_references=True,include_hard_management_references=True)
assets=ar.get_assets_by_path('/Game',recursive=True,include_only_on_disk_assets=True)
packages=sorted(set(str(a.package_name) for a in assets))
maps=sorted('/Game/'+p.relative_to(root/'Content').with_suffix('').as_posix() for p in (root/'Content').rglob('*.umap'))
code=set()
for folder in ('Source','Config','Plugins'):
 for p in (root/folder).rglob('*'):
  if p.suffix not in ('.cpp','.h','.inl','.ini','.uplugin'):continue
  for path in re.findall(r'/Game/[A-Za-z0-9_/]+',p.read_text(encoding='utf-8',errors='ignore')):
   if path in packages:code.add(path)
roots=sorted(set(maps)|code);reachable=set();queue=list(roots)
while queue:
 p=queue.pop()
 if p in reachable:continue
 reachable.add(p)
 queue.extend(str(x) for x in (ar.get_dependencies(p,opts) or []) if str(x).startswith('/Game/') and str(x) not in reachable)
folders=[]
for folder in sorted((root/'Content').iterdir()):
 if not folder.is_dir():continue
 files=[p for p in folder.rglob('*') if p.is_file()]
 prefix='/Game/'+folder.name+'/'
 contained=[p for p in packages if p.startswith(prefix)]
 needed=[p for p in contained if p in reachable]
 folders.append(dict(folder=folder.name,file_count=len(files),asset_count=len(contained),reachable_count=len(needed),reachable=needed,unreachable=[p for p in contained if p not in reachable]))
shared={p:[str(x) for x in (ar.get_referencers(p,opts) or [])] for p in packages if p.startswith('/Game/ProcessPlant_20261029/')}
result=dict(maps=maps,code_roots=sorted(code),packages=len(packages),reachable=len(reachable),unreachable=sorted(set(packages)-reachable),folders=folders,old_material_referencers=shared)
(out/'asset_audit.json').write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf-8')
unreal.log('DEMO_ASSET_AUDIT_COMPLETE '+str(len(packages))+' assets; '+str(len(set(packages)-reachable))+' unreachable')
