import unreal as u
path='/Game/L_BulkPrototype'
assert not u.EditorAssetLibrary.does_asset_exist(path),'Refusing to overwrite level'
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);assert levels.new_level(path)
a=u.get_editor_subsystem(u.EditorActorSubsystem)
cls=u.load_class(None,'/Script/VirtualGameTeamProj.PowderBulkPrototype');game=u.load_class(None,'/Script/VirtualGameTeamProj.PowderBulkGameMode');assert cls and game
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world();world.get_world_settings().set_editor_property('default_game_mode',game)
actor=a.spawn_actor_from_class(cls,u.Vector(0,0,250),u.Rotator(pitch=-20,yaw=0,roll=0));actor.set_actor_label('BulkPrototype_30kg_0p1x');actor.reset_simulation();assert not actor.get_editor_property('error')
light=a.spawn_actor_from_class(u.DirectionalLight,u.Vector(0,0,600),u.Rotator(pitch=-50,yaw=-30,roll=0));light.light_component.set_editor_property('intensity',5.)
a.spawn_actor_from_class(u.SkyLight,u.Vector(0,0,500));a.spawn_actor_from_class(u.SkyAtmosphere,u.Vector())
assert levels.save_current_level();u.log('OPT28_BULK_LEVEL_CREATED')
