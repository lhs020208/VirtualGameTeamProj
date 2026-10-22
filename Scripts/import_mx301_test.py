import unreal as u
from pathlib import Path
for name in ['SM_MX301_TestVessel','SM_MX301_TestRotor']:
 t=u.AssetImportTask();t.filename='D:/GitHub/VirtualGameTeamProj/SourceAssets/Granular/'+name+'.fbx';t.destination_path='/Game/DEM';t.destination_name=name;t.automated=True;t.save=True;t.replace_existing=True
 o=u.FbxImportUI();o.import_mesh=True;o.import_materials=False;o.import_textures=False;o.import_as_skeletal=False;o.mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH;o.static_mesh_import_data.combine_meshes=True;o.static_mesh_import_data.auto_generate_collision=False;t.options=o
 u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t]);assert u.load_asset('/Game/DEM/'+name)
path='/Game/L_MX301_Test';assert not u.EditorAssetLibrary.does_asset_exist(path),'Refuse overwrite existing test level'
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);assert levels.new_level(path)
actors=u.get_editor_subsystem(u.EditorActorSubsystem);cls=u.load_class(None,'/Script/VirtualGameTeamProj.PowderGranularMixer')
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world();world.get_world_settings().set_editor_property('default_game_mode',u.load_class(None,'/Script/VirtualGameTeamProj.PowderBulkGameMode'))
a=actors.spawn_actor_from_class(cls,u.Vector());a.set_editor_property('particle_count',50000);a.set_editor_property('test_mixer_asset',True);a.set_editor_property('time_step',.00025);a.set_editor_property('target_rate',.1);a.set_actor_label('MX301_NONFINAL_TEST_50000_64_8kg_closed')
light=actors.spawn_actor_from_class(u.DirectionalLight,u.Vector(0,0,800),u.Rotator(pitch=-55,yaw=-25));light.light_component.set_editor_property('intensity',3.)
actors.spawn_actor_from_class(u.SkyLight,u.Vector(0,0,800));actors.spawn_actor_from_class(u.SkyAtmosphere,u.Vector());assert levels.save_current_level();u.log('MX301_TEST_IMPORTED')
