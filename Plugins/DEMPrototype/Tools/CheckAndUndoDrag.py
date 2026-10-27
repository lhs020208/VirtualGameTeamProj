import unreal,json
from pathlib import Path
out=Path('C:/GitHub/VirtualGameTeamProj/Plugins/DEMPrototype/CommonValidation')
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
getactors=lambda:unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor)
before=set(json.loads((out/'drag_before.json').read_text()))
new=[a for a in getactors() if a.get_path_name() not in before]
assert len(new)==1 and 'BP_DEMEmitter' in new[0].get_class().get_name(), 'Unexpected actor delta; stop for inspection'
a=new[0];ism=a.get_components_by_class(unreal.InstancedStaticMeshComponent)[0]
r={'added_class':a.get_class().get_path_name(),'settings':a.get_editor_property('particle_settings').get_path_name(),'requested_count':a.get_editor_property('particle_count'),'preview_instance_count':ism.get_instance_count(),'status':a.get_editor_property('generation_status'),'position':str(a.get_actor_location()),'mouse_drag_added_one_actor':True}
unreal.SystemLibrary.execute_console_command(w,'TRANSACTION UNDO')
r['undo_restored_actor_set']=set(x.get_path_name() for x in getactors())==before
(out/'drag_result.json').write_text(json.dumps(r,indent=2))
assert r['undo_restored_actor_set'],'Undo did not restore actor set; do not save'
unreal.log('DEM_MOUSE_DRAG_PASS_AND_UNDONE '+json.dumps(r))
