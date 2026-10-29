"""Static editor inspection only; safe to run without PIE or simulation."""
import json, math
from pathlib import Path
import unreal as u
ROOT='/Game/ProcessPlant_20261029'
LEVEL=ROOT+'/L_ProcessPlant_Assembly'
OUT=Path('D:/GitHub/VirtualGameTeamProj/Docs/ProcessPlant_20261029')
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
assert levels.load_level(LEVEL)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
rows=[]
for actor in actors.get_all_level_actors():
    transform=actor.get_actor_transform()
    parent=actor.get_attach_parent_actor()
    meshes=[]
    for c in actor.get_components_by_class(u.StaticMeshComponent):
        if not c.static_mesh: continue
        mesh=c.static_mesh
        meshes.append(dict(mesh=mesh.get_path_name(),slots=c.get_num_materials(),
            materials=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())],
            collision=str(c.get_collision_enabled()),no_collision=c.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION,physics=c.is_simulating_physics(),
            simple_collision_count=u.get_editor_subsystem(u.StaticMeshEditorSubsystem).get_simple_collision_count(mesh)))
    rows.append(dict(label=actor.get_actor_label(),type=actor.get_class().get_name(),
        parent=parent.get_actor_label() if parent else None,
        origin=list(transform.transform_location(u.Vector()).to_tuple()),
        basis=[list(transform.transform_location(v).to_tuple()) for v in [u.Vector(1,0,0),u.Vector(0,1,0),u.Vector(0,0,1)]],meshes=meshes))
inventory=u.EditorAssetLibrary.list_assets(ROOT,recursive=True,include_folder=False)
counts={}
for path in inventory:
    obj=u.load_asset(path)
    assert obj,path
    name=obj.get_class().get_name()
    counts[name]=counts.get(name,0)+1
master=u.load_asset(ROOT+'/Materials/M_PlantSimple')
if master:
    assert master.get_editor_property('shading_model')==u.MaterialShadingModel.MSM_UNLIT
    for path in u.EditorAssetLibrary.list_assets(ROOT+'/Assets/Materials',recursive=True,include_folder=False):
        assert u.load_asset(path).get_editor_property('parent')==master
assert sum(len(r['meshes']) for r in rows)==1717
assert all(not m['physics'] and m['no_collision'] and m['simple_collision_count']==0
    and all(m['materials']) for r in rows for m in r['meshes'])
assert not any('Powder' in r['type'] or 'Emitter' in r['type'] for r in rows)
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
assert world.get_world_settings().get_editor_property('default_game_mode')==u.GameModeBase.static_class()
# Editor viewport only. This does not create or run a game world.
camera=u.Vector(570,470,430)
target=u.Vector(150,-50,200)
rotation=u.MathLibrary.find_look_at_rotation(camera,target)
u.get_editor_subsystem(u.UnrealEditorSubsystem).set_level_viewport_camera_info(camera,rotation)
assert levels.save_current_level()
(OUT/'reload_validation.json').write_text(json.dumps(dict(level=LEVEL,asset_classes=counts,
    actors=rows,all_meshes_no_collision=True,no_powder_actors=True,game_mode='GameModeBase',
    viewport_camera=list(camera.to_tuple()),simulation_executed=False),ensure_ascii=False,indent=2),encoding='utf-8')
u.log('PROCESS_PLANT_STATIC_VALIDATION_COMPLETE')
