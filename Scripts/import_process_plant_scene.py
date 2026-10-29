"""Editor-only scene import. Never starts PIE, simulation, or a powder actor."""
import json
from pathlib import Path
import unreal as u

ROOT = '/Game/ProcessPlant_20261029'
LEVEL = ROOT + '/L_ProcessPlant_Assembly'
SOURCE = 'D:/GitHub/가상현실게임/Exports/ProcessPlant_20261029/ProcessPlant_Assembly.fbx'
OUT = Path('D:/GitHub/VirtualGameTeamProj/Docs/ProcessPlant_20261029')
OUT.mkdir(parents=True, exist_ok=True)
u.AssetRegistryHelpers.get_asset_registry().search_all_assets(synchronous_search=True)
assert not u.EditorAssetLibrary.does_directory_exist(ROOT + '/Assets'), 'Refusing existing imported assets'
levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
if u.EditorAssetLibrary.does_asset_exist(LEVEL):
    assert levels.load_level(LEVEL)
    assert not u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors(), 'Refusing nonempty level'
else:
    assert levels.new_level(LEVEL)
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', u.GameModeBase)

# Duplicate engine presets; never modify the shared engine/default pipelines.
assets = u.EditorAssetLibrary.duplicate_asset(
    '/Interchange/Pipelines/DefaultFBXOBJSceneAssetsPipeline', ROOT + '/ImportSettings/PlantAssets')
scene = u.EditorAssetLibrary.duplicate_asset(
    '/Interchange/Pipelines/DefaultSceneLevelPipeline', ROOT + '/ImportSettings/PlantScene')
assert assets and scene
assets.set_editor_property('asset_type_sub_folders', True)
assets.set_editor_property('import_offset_uniform_scale', 1.0)
common = assets.get_editor_property('common_meshes_properties')
common.set_editor_property('bake_meshes', False)
common.set_editor_property('bake_pivot_meshes', False)
mesh = assets.get_editor_property('mesh_pipeline')
mesh.set_editor_property('combine_static_meshes_behavior', u.InterchangeCombineStaticMeshesBehavior.DO_NOT_COMBINE)
mesh.set_editor_property('import_skeletal_meshes', False)
mesh.set_editor_property('collision', False)
assets.get_editor_property('animation_pipeline').set_editor_property('import_animations', False)
scene.set_editor_property('scene_hierarchy_type', u.InterchangeSceneHierarchyType.CREATE_LEVEL_ACTORS)
for obj in (assets, scene):
    u.EditorAssetLibrary.save_loaded_asset(obj)
params = u.ImportAssetParameters()
params.is_automated = True
params.replace_existing = False
params.override_pipelines = [u.SoftObjectPath(obj.get_path_name()) for obj in (assets, scene)]
manager = u.InterchangeManager.get_interchange_manager_scripted()
source = u.InterchangeManager.create_source_data(SOURCE)
assert manager.import_scene(ROOT + '/Assets', source, params), 'Scene import failed'
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
rows = []
for actor in actors.get_all_level_actors():
    comps = actor.get_components_by_class(u.StaticMeshComponent)
    for comp in comps:
        if not comp.static_mesh:
            continue
        comp.set_simulate_physics(False)
        comp.set_collision_profile_name('NoCollision')
        comp.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
        comp.set_editor_property('cast_shadow', False)
        parent = actor.get_attach_parent_actor()
        origin, extent = actor.get_actor_bounds(False)
        rows.append(dict(label=actor.get_actor_label(), actor=actor.get_path_name(),
            parent=parent.get_actor_label() if parent else None,
            location=list(actor.get_actor_location().to_tuple()),
            rotation=str(actor.get_actor_rotation()), scale=list(actor.get_actor_scale3d().to_tuple()),
            mesh=comp.static_mesh.get_path_name(),
            materials=[comp.get_material(i).get_path_name() if comp.get_material(i) else None for i in range(comp.get_num_materials())],
            bounds_origin=list(origin.to_tuple()), bounds_extent=list(extent.to_tuple()),
            collision=str(comp.get_collision_enabled()), simulate_physics=comp.is_simulating_physics()))
assert len(rows) == 1717, f'Expected 1717 mesh nodes; found {len(rows)}'
light=actors.spawn_actor_from_class(u.DirectionalLight,u.Vector(0,0,1200),u.Rotator(-40,-35,0))
light.set_actor_label('Preview_Sun')
light.light_component.set_editor_property('intensity', 3.0)
light.light_component.set_editor_property('cast_shadows', False)
actors.spawn_actor_from_class(u.SkyLight,u.Vector(0,0,1000))
actors.spawn_actor_from_class(u.SkyAtmosphere,u.Vector())
assert u.EditorAssetLibrary.save_directory(ROOT, only_if_is_dirty=True, recursive=True)
assert levels.save_current_level()
report=dict(level=LEVEL,source=SOURCE,mesh_actor_count=len(rows),actors=rows,
    settings=dict(importer='Interchange synchronous scene import',hierarchy='CreateLevelActors',
        combine=False,bake_meshes=False,bake_pivot=False,scale=1,collision=False,animation=False),
    limitation='Visual-only FBX; no hidden collision or internal-volume geometry. No simulation run.')
(OUT/'import_report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
u.log('PROCESS_PLANT_IMPORT_SAVED '+LEVEL)
