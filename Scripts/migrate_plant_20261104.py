import unreal as u,json
from pathlib import Path
p=Path('D:/GitHub/VirtualGameTeamProj');root='/Game/ProcessPlant_20261104';target='/Game/ProcessPlant_20261103/L_ProcessPlant_ParticleSetup'
ls=u.get_editor_subsystem(u.LevelEditorSubsystem);es=u.get_editor_subsystem(u.EditorActorSubsystem)
w=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
assert w.get_path_name()==target+'.L_ProcessPlant_ParticleSetup'
assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert ls.new_level(root+'/L_ImportStaging')
params=u.ImportAssetParameters();params.is_automated=True
params.override_pipelines=[u.SoftObjectPath('/Game/ProcessPlant_20261029/ImportSettings/'+n+'.'+n) for n in ['PlantAssets','PlantScene']]
mgr=u.InterchangeManager.get_interchange_manager_scripted()
assert mgr.import_scene(root+'/Assets',mgr.create_source_data(str(p/'SourceAssets/ProcessPlant_20261104/ProcessPlant_20261104.fbx')),params)
new={a.get_actor_label():a.static_mesh_component.static_mesh.get_path_name() for a in es.get_all_level_actors() if isinstance(a,u.StaticMeshActor)}
assert len(new)>2000
assert u.EditorAssetLibrary.save_directory(root,only_if_is_dirty=True,recursive=True)
assert ls.load_level(target)
old={a.get_actor_label():a for a in es.get_all_level_actors() if isinstance(a,u.StaticMeshActor)}
assert set(old).issubset(new)
assert set(new)-set(old)=={'BV122_ClosedSeal',*[f'V101{x}_ClosedSeal' for x in 'ABCDEF']}
# ClosedSeal helpers duplicate the live gate visuals; do not instantiate them.
records=[]
for name,a in old.items():
 c=a.static_mesh_component;records.append({'actor':name,'old':c.static_mesh.get_path_name(),'new':new[name],'transform':str(a.get_actor_transform())})
 c.set_static_mesh(u.load_asset(new[name]))
# Preserve actor identity, transforms, material overrides, display references, controls and settings.
tests=[a for a in es.get_all_level_actors() if a.get_class()==u.load_class(None,'/Script/VirtualGameTeamProj.PowderPlantTest')];assert len(tests)==1
t=tests[0];t.set_editor_property('data_directory','SourceAssets/ProcessPlant_20261104/Simulation');t.reset_simulation();assert not str(t.get_editor_property('error'))
assert t.get_editor_property('particle_count')==20000
assert ls.save_current_level()
(p/'Docs/ProcessPlant_20261104/migration.json').write_text(json.dumps({'level':target,'count':len(records),'actors':records},ensure_ascii=False,indent=2),encoding='utf8')
