import unreal,json
from pathlib import Path
s=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not s.get_game_world()
w=s.get_editor_world()
assert w.get_path_name()=='/Game/DEM/DEM_Integration.DEM_Integration'
a=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor)
Path('C:/GitHub/VirtualGameTeamProj/Plugins/DEMPrototype/CommonValidation/drag_before.json').write_text(json.dumps(sorted(x.get_path_name() for x in a)))
unreal.EditorAssetLibrary.sync_browser_to_objects(['/Game/DEM/Emitters/BP_DEMEmitter'])
unreal.log('DEM_DRAG_TEST_READY')
