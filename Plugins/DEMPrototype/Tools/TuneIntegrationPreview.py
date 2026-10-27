import unreal
s=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not s.get_game_world()
w=s.get_editor_world()
assert w.get_path_name()=='/Game/DEM/DEM_Integration.DEM_Integration'
for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
    if 'BP_DEMWorld' in a.get_class().get_name():
        a.set_editor_property('speed_multiplier',.05)
        a.set_editor_property('max_substeps',10000)
    if 'BP_DEMPlaneBoundary' in a.get_class().get_name() or 'BP_DEMEmitter' in a.get_class().get_name():
        for arrow in a.get_components_by_class(unreal.ArrowComponent):
            arrow.set_editor_property('arrow_size',.06)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
unreal.log('DEM_PREVIEW_TUNED: speed .05, maxsubsteps 10000; smaller editor arrows')
