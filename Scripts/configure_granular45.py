import unreal as u
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
for path,dt,rate,mx in [('/Game/L_GranularMixer',.000375,.2,False),('/Game/L_MX301_Test',.000375,.18,True)]:
 assert levels.load_level(path)
 matches=[a for a in actors.get_all_level_actors() if isinstance(a,u.PowderGranularMixer)];assert len(matches)==1
 a=matches[0];a.set_editor_property('particle_count',50000);a.set_editor_property('time_step',dt);a.set_editor_property('target_rate',rate);a.set_editor_property('solver_batch_size',32);a.set_editor_property('test_mixer_asset',mx)
 position=u.Vector(170,-210,115) if mx else u.Vector(115,-160,95)
 u.get_editor_subsystem(u.UnrealEditorSubsystem).set_level_viewport_camera_info(position,u.MathLibrary.find_look_at_rotation(position,u.Vector(0,0,-5)))
 assert levels.save_current_level()
u.log('GRANULAR45_LEVEL_DEFAULTS_SAVED')
