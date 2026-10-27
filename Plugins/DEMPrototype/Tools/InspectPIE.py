"""Read PIE state and use the existing test camera, without saving any assets."""
import unreal, json
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
result={'world':str(world),'managers':[],'cameras':[]}
if world:
    actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
    for a in actors:
        if 'DEMWorld' in a.get_class().get_name():
            row={}
            for k in ['status','simulation_time','particle_count','lag','running','reset_required']:
                try: row[k]=str(a.get_editor_property(k))
                except Exception as e: row[k]=str(e)
            result['managers'].append(row)
    pc=unreal.GameplayStatics.get_player_controller(world,0)
    cameras=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CameraActor)
    result['cameras']=[{'name':a.get_name(),'position':str(a.get_actor_location()),'rotation':str(a.get_actor_rotation())} for a in cameras]
    result['controller']=str(pc)
    if pc and cameras:
        result['previous_view_target']=str(pc.get_view_target())
        pc.set_view_target_with_blend(cameras[0],0)
        result['new_view_target']=str(pc.get_view_target())
    unreal.SystemLibrary.execute_console_command(world,'viewmode unlit',pc)
with open('C:/GitHub/VirtualGameTeamProj/Plugins/DEMPrototype/CommonValidation/pie_state.json','w') as f: json.dump(result,f,indent=2)
unreal.log('DEM_PIE_VIEW_READY '+str(result))
