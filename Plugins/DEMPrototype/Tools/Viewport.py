"""Safe default for the UI helper: report world names only, without changing state."""
import unreal
s=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
unreal.log('DEM editor world: '+str(s.get_editor_world()))
unreal.log('DEM PIE world: '+str(s.get_game_world()))
