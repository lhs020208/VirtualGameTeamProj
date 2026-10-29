import unreal as u
path='/Game/DEM/Tests/L_CoupledPowder_Minimum'
if u.EditorAssetLibrary.does_asset_exist(path):
    raise RuntimeError('Refusing to overwrite existing coupled level')
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
assert levels.new_level(path)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
cls=u.load_class(None,'/Script/VirtualGameTeamProj.PowderCoupledTest')
assert cls
actor=actors.spawn_actor_from_class(cls,u.Vector())
actor.set_actor_label('CoupledPowder_CPU_Reference_NotCalibrated')
actor.reset_simulation()
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode',u.load_class(None,'/Script/VirtualGameTeamProj.PowderBulkGameMode'))
light=actors.spawn_actor_from_class(u.DirectionalLight,u.Vector(0,0,500),u.Rotator(pitch=-55,yaw=-25))
light.light_component.set_editor_property('cast_shadows',False)
assert levels.save_current_level()
u.log('COUPLED_MINIMUM_LEVEL_CREATED')
