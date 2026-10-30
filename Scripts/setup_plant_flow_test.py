"""Create a separate editable plant test level; no PIE or simulation in commandlet."""
import unreal as u,json
from pathlib import Path
root=Path('D:/GitHub/VirtualGameTeamProj')
base='/Game/ProcessPlant_20261029'
target=base+'/L_ProcessPlant_FlowTest'
u.AssetRegistryHelpers.get_asset_registry().search_all_assets(synchronous_search=True)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
if not u.EditorAssetLibrary.does_asset_exist(target):assert u.EditorAssetLibrary.duplicate_asset(base+'/L_ProcessPlant_Assembly_Display',target)
assert levels.load_level(target)
inventory={a.get_actor_label():a for a in actors.get_all_level_actors()}
generated_names={'Test_PL_cuff_connection','Test_BV122_seal','Test_BV272_seal'}
old_generated={a for n,a in inventory.items() if n in generated_names}
if old_generated:
    display=inventory['Plant_DisplayControls']
    display.set_editor_property('parts',[p for p in display.get_editor_property('parts') if p.get_editor_property('actor') not in old_generated])
    for a in old_generated:actors.destroy_actor(a)
if True:
    pipeline=u.EditorAssetLibrary.load_asset(base+'/ImportSettings/PlantAssets')
    scene=u.EditorAssetLibrary.load_asset(base+'/ImportSettings/PlantScene')
    params=u.ImportAssetParameters();params.is_automated=True;params.replace_existing=True
    params.override_pipelines=[u.SoftObjectPath(pipeline.get_path_name()),u.SoftObjectPath(scene.get_path_name())]
    source=u.InterchangeManager.create_source_data(str(root/'SourceAssets/ProcessPlantPhysics/TestSeals.fbx'))
    assert u.InterchangeManager.get_interchange_manager_scripted().import_scene(base+'/TestSeals',source,params)
    inventory={a.get_actor_label():a for a in actors.get_all_level_actors()}
cls=u.load_class(None,'/Script/VirtualGameTeamProj.PowderPlantTest');assert cls
test=next((a for a in actors.get_all_level_actors() if a.get_class()==cls),None)
if not test:test=actors.spawn_actor_from_class(cls,u.Vector());test.set_actor_label('Plant_Pressure_Grains_Test')
test.reset_simulation();assert not test.get_editor_property('error'),test.get_editor_property('error')
assert test.get_editor_property('particle_count')==30000
# Keep the supplied source level unchanged; close its displaced inspection cover here.
for name in ['WH101_Inspection_Lid','WH101_Inspection_Handle']:
 if name in inventory:
  a=inventory[name]
  if not a.actor_has_tag('PlantTestLidClosed'):
   a.set_actor_location(a.get_actor_location()+u.Vector(19.5,-19.5,183.7),False,False)
   a.tags=list(a.tags)+['PlantTestLidClosed']
# Source scene renderer and solver use the same shaft reference frame.
rotor=[a for n,a in inventory.items() if n.startswith('MX-301_') and ('Paddle' in n or 'Shaft' in n) and 'Drive' not in n and 'Support' not in n]
for a in rotor:
 for c in a.get_components_by_class(u.SceneComponent):c.set_mobility(u.ComponentMobility.MOVABLE)
controller=inventory['Plant_DisplayControls']
parts=list(controller.get_editor_property('parts'))
existing={p.get_editor_property('actor') for p in parts}
material=u.EditorAssetLibrary.load_asset(base+'/Materials/M_PlantCutaway')
for name,a in inventory.items():
 if name.startswith('Test_') and a.get_component_by_class(u.StaticMeshComponent):
  comp=a.get_component_by_class(u.StaticMeshComponent);comp.set_material(0,material);comp.set_collision_enabled(u.CollisionEnabled.NO_COLLISION);comp.set_editor_property('cast_shadow',False)
  mesh=comp.static_mesh
  settings=mesh.get_editor_property('nanite_settings');settings.set_editor_property('enabled',False);mesh.set_editor_property('nanite_settings',settings)
  if a not in existing:
   part=u.PlantVisualPart();part.set_editor_property('actor',a);part.set_editor_property('cutaway',True)
   origin,extent=a.get_actor_bounds(False);transform=a.get_actor_transform()
   part.set_editor_property('local_plane_origin',u.MathLibrary.inverse_transform_location(transform,origin));part.set_editor_property('local_plane_normal',u.Vector(0,1,0));parts.append(part)
controller.set_editor_property('parts',parts)
controller.set_editor_property('show_frames',False);controller.show_cutaway()
camera=inventory['Plant_OverviewCamera'];camera.set_actor_location(u.Vector(650,950,550),False,False);camera.set_actor_rotation(u.MathLibrary.find_look_at_rotation(camera.get_actor_location(),u.Vector(110,-40,200)),False)
u.get_editor_subsystem(u.UnrealEditorSubsystem).set_level_viewport_camera_info(camera.get_actor_location(),camera.get_actor_rotation())
assert levels.save_current_level()
u.EditorAssetLibrary.save_directory(base+'/TestSeals',only_if_is_dirty=True,recursive=True)
(root/'Docs/ProcessPlantPhysics/level_setup.json').write_text(json.dumps(dict(level=target,particles=test.get_editor_property('particle_count'),rotor_parts=len(rotor),actor=test.get_path_name()),indent=2),encoding='utf-8')
u.log('PLANT_FLOW_TEST_READY '+target)
