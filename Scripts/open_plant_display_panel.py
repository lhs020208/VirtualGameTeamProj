"""Open the GUI in the current editor world without starting simulation."""
import unreal as u
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
controller=next(a for a in actors.get_all_level_actors() if a.get_class().get_name()=='PlantVisibilityController')
actors.set_selected_level_actors([controller])
controller.open_display_panel()
