"""Import only into the independent November 10 level (Unreal Python)."""
import unreal as u,json
from pathlib import Path
p=Path('D:/GitHub/VirtualGameTeamProj');root='/Game/ProcessPlant_20261110_LowPoly'
target=root+'/L_ProcessPlant_LowPoly'
ls=u.get_editor_subsystem(u.LevelEditorSubsystem);es=u.get_editor_subsystem(u.EditorActorSubsystem)
assert not u.EditorAssetLibrary.does_asset_exist(target),'Preserve existing level; choose a new destination for another import'
assert ls.new_level(target)
params=u.ImportAssetParameters();params.is_automated=True
params.override_pipelines=[u.SoftObjectPath('/Game/ProcessPlant_20261029/ImportSettings/'+n+'.'+n) for n in ['PlantAssets','PlantScene']]
mgr=u.InterchangeManager.get_interchange_manager_scripted()
assert mgr.import_scene(root+'/Assets',mgr.create_source_data(str(p/'SourceAssets/ProcessPlant_20261110_LowPoly/ProcessPlant_LowPoly.fbx')),params)
actors=[a for a in es.get_all_level_actors() if isinstance(a,u.StaticMeshActor)]
assert len(actors)>2000
records=[dict(name=a.get_actor_label(),mesh=a.static_mesh_component.static_mesh.get_path_name(),transform=str(a.get_actor_transform())) for a in actors]
assert u.EditorAssetLibrary.save_directory(root,only_if_is_dirty=True,recursive=True)
assert ls.save_current_level()
(p/'Docs/ProcessPlant_20261110_LowPoly/import.json').write_text(json.dumps(dict(level=target,actors=records),ensure_ascii=False,indent=2),encoding='utf8')
print('LOWPOLY_IMPORT_COMPLETE',len(actors))
