"""Create an isolated task-19 fixture without modifying L_Main."""
import unreal as u
path='/Game/L_MeshTest'
assert not u.EditorAssetLibrary.does_asset_exist(path), 'Refusing to overwrite an existing level'
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
assert levels.new_level(path)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
cls=u.load_class(None,'/Script/VirtualGameTeamProj.PowderClumpTest')
game=u.load_class(None,'/Script/VirtualGameTeamProj.PowderClumpGameMode')
assert cls and game
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode',game)
actor=actors.spawn_actor_from_class(cls,u.Vector(0,0,0));actor.set_actor_label('MeshExperiment_Ramp_OpenTube_6Bodies')
actor.set_editor_property('triangle_mesh_rig',True)
actor.reset_simulation()
assert not actor.get_editor_property('error'), str(actor.get_editor_property('error'))
light=actors.spawn_actor_from_class(u.DirectionalLight,u.Vector(0,0,200),u.Rotator(pitch=-50,yaw=-30,roll=0))
light.light_component.set_editor_property('intensity',3.0)
actors.spawn_actor_from_class(u.SkyLight,u.Vector(0,0,200))
actors.spawn_actor_from_class(u.SkyAtmosphere,u.Vector(0,0,0))
assert levels.save_current_level()
u.log('TASK19_LEVEL_CREATED /Game/L_MeshTest')
