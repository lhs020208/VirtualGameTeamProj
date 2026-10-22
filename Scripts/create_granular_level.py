import unreal as u
path='/Game/L_GranularMixer'
assert not u.EditorAssetLibrary.does_asset_exist(path),'Refusing overwrite'
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);assert levels.new_level(path)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
cls=u.load_class(None,'/Script/VirtualGameTeamProj.PowderGranularMixer')
game=u.load_class(None,'/Script/VirtualGameTeamProj.PowderBulkGameMode')
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world();world.get_world_settings().set_editor_property('default_game_mode',game)
a=actors.spawn_actor_from_class(cls,u.Vector());a.set_actor_label('GranularMixer_30000_Continuous_GPU')
light=actors.spawn_actor_from_class(u.DirectionalLight,u.Vector(0,0,800),u.Rotator(pitch=-55,yaw=-25));light.light_component.set_editor_property('intensity',3.)
actors.spawn_actor_from_class(u.SkyLight,u.Vector(0,0,800));actors.spawn_actor_from_class(u.SkyAtmosphere,u.Vector())
assert levels.save_current_level();u.log('GRANULAR_LEVEL_CREATED')
