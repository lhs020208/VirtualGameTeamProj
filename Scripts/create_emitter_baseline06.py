import unreal as u
path='/Game/DEM/Tests/L_MX301_LegacyBaseline'
if not u.EditorAssetLibrary.does_asset_exist(path):
 b=u.EditorAssetLibrary.duplicate_asset('/Game/L_MX301_Test',path)
 assert b and u.EditorAssetLibrary.save_loaded_asset(b)
l=u.get_editor_subsystem(u.LevelEditorSubsystem);assert l.load_level(path)
a=u.get_editor_subsystem(u.EditorActorSubsystem)
for actor in a.get_all_level_actors():
 if actor.get_class().get_name()=='PowderGranularMixer':
  actor.set_editor_property('use_emitters',False);actor.set_editor_property('emitters',[]);actor.set_editor_property('particle_settings',None)
for actor in a.get_all_level_actors():
 if actor.get_class().get_name()=='BP_DEMEmitter_C':a.destroy_actor(actor)
assert l.save_current_level()
