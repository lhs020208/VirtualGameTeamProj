import unreal as u
level=u.get_editor_subsystem(u.LevelEditorSubsystem)
assert level.load_level('/Game/L_GranularMixer')
cls=u.load_class(None,'/Script/VirtualGameTeamProj.PowderGranularMixer')
matches=[a for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors() if a.get_class()==cls]
assert len(matches)==1
a=matches[0];a.set_editor_property('particle_count',50000);a.set_editor_property('target_rate',.1);a.set_actor_label('GranularMixer_50000_Continuous_GPU')
assert level.save_current_level();u.log('FINAL_GRANULAR_LEVEL_50000_SAVED')
