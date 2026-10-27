"""Improve only the generated Integration map's viewing setup; physics assets stay untouched."""
import unreal, json, hashlib
from pathlib import Path
root=Path('C:/GitHub/VirtualGameTeamProj')
map_file=root/'Content/DEM/DEM_Integration.umap'
expected=hashlib.sha256((root/'Plugins/DEMPrototype/CommonValidation/Before/DEM_Integration_before_view_fix.umap').read_bytes()).hexdigest()
assert hashlib.sha256(map_file.read_bytes()).hexdigest()==expected, 'Integration changed on disk: inspect before editing'
sub=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not sub.get_game_world(), 'Stop PIE first'
world=sub.get_editor_world()
assert world.get_path_name()=='/Game/DEM/DEM_Integration.DEM_Integration', 'Wrong map'
actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
manager=next(a for a in actors if 'BP_DEMWorld' in a.get_class().get_name())
manager.set_editor_property('speed_multiplier',0.05)
manager.set_editor_property('max_substeps',10000)
camera=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CameraActor)[0]
camera.set_editor_property('auto_activate_for_player',unreal.AutoReceiveInput.PLAYER0)
camera.set_actor_label('DEM observation camera - optional test map view')
volumes=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PostProcessVolume)
assert not volumes, 'Unexpected existing post process; inspect first'
volume=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector())
volume.set_actor_label('DEM test exposure - EV100 0')
volume.set_editor_property('unbound',True)
s=volume.get_editor_property('settings')
for name,value in [('override_auto_exposure_min_brightness',True),('override_auto_exposure_max_brightness',True),('override_auto_exposure_bias',True),('auto_exposure_min_brightness',0.0),('auto_exposure_max_brightness',0.0),('auto_exposure_bias',0.0)]:
    s.set_editor_property(name,value)
volume.set_editor_property('settings',s)
sub.set_level_viewport_camera_info(camera.get_actor_location(),camera.get_actor_rotation())
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
level.editor_invalidate_viewports()
assert level.save_current_level(), 'Save failed'
unreal.log('DEM_INTEGRATION_VIEW_FIXED: camera Player0, EV100=0, speed=0.05, max_substeps=10000; only Integration saved')
