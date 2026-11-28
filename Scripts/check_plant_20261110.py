import json,time,traceback
from pathlib import Path
import unreal as u
out=Path('D:/GitHub/VirtualGameTeamProj/Docs/ProcessPlant_20261110_LowPoly/play_validation.json')
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
assert levels.load_level('/Game/ProcessPlant_20261110_LowPoly/L_ProcessPlant_LowPoly')
u.EditorPythonScripting.set_keep_python_script_alive(True)
state=0;start=time.monotonic();report={}
def finish(error=None):
 if error:report['failure']=error
 out.write_text(json.dumps(report,indent=2));u.unregister_slate_post_tick_callback(handle)
 levels.editor_request_end_play();u.EditorPythonScripting.set_keep_python_script_alive(False);u.SystemLibrary.quit_editor()
def tick(dt):
 global state,initial,actors,p,discs,blade,display,last
 try:
  if time.monotonic()-start>180:raise RuntimeError('timeout at '+str(state))
  if state==0:
   if not levels.is_in_play_in_editor():return
   worlds=u.EditorLevelLibrary.get_pie_worlds(False)
   if not worlds:return
   actors={a.get_actor_label():a for a in u.GameplayStatics.get_all_actors_of_class(worlds[0],u.Actor)}
   p=actors['Plant_Pressure_Grains_LowPoly'];display=actors['Plant_DisplayControls'];display.show_full()
   if p.get_editor_property('error'):raise RuntimeError(str(p.get_editor_property('error')))
   if p.get_editor_property('physical_seconds')<=0:return
   discs=[actors['V101'+letter+'_Disc'] for letter in 'ABCDEF']
   blade=actors['MX-301_Paddle_01'];initial=blade.get_actor_rotation().roll
   report['mixer_pivot_m']=list(p.get_editor_property('mixer_pivot_m').to_tuple());assert abs(report['mixer_pivot_m'][2]-1.105)<1e-6
   report['initial_particle_groups']=[c.get_instance_count() for c in p.get_components_by_class(u.InstancedStaticMeshComponent) if c.get_instance_count()]
   assert report['initial_particle_groups']==[10000,10000]
   assert all(not a.get_editor_property('hidden') for a in discs)
   for i in range(6):p.toggle_supplier(i)
   p.toggle_weighing_hopper();p.toggle_receiver();p.toggle_mixer()
   state=1;last=time.monotonic()
  elif state==1:
   if str(p.get_editor_property('error')):raise RuntimeError(str(p.get_editor_property('error')))
   report['observed_discs']=[bool(a.get_editor_property('hidden')) for a in discs];report['roll_degrees']=abs(blade.get_actor_rotation().roll-initial);report['sim_time']=p.get_editor_property('physical_seconds')
   if not all(report['observed_discs']):return
   if abs(blade.get_actor_rotation().roll-initial)<.005:return
   report['mixer_axis_checks']={a.get_actor_label():list(a.get_actor_location().to_tuple()) for a in actors.values() if a.get_actor_label().startswith('MX-301_Shaft')}
   report['open_suppliers']=[a.get_actor_label() for a in discs];report['paddle_rotation_degrees']=abs(blade.get_actor_rotation().roll-initial)
   report['outlet_visibility']={n:bool(a.get_editor_property('hidden')) for n,a in actors.items() if n in ['BV272_210_Disc','ASSUMED_VT_MovingGate']}
   assert len(report['outlet_visibility'])==2 and all(report['outlet_visibility'].values())
   for i in range(6):p.toggle_supplier(i)
   p.toggle_weighing_hopper();p.toggle_receiver();p.toggle_mixer()
   state=2
  elif state==2:
   if any(a.get_editor_property('hidden') for a in discs) or any(actors[n].get_editor_property('hidden') for n in report['outlet_visibility']):return
   report['closed_after_toggle']=True;old=p.get_editor_property('physical_seconds')
   p.reset_simulation();report['reset_time']=p.get_editor_property('physical_seconds');assert report['reset_time']==0
   display.hide_equipment();display.show_cutaway();display.set_frames_visible(False)
   state=3
  elif state==3:
   if str(p.get_editor_property('error')):raise RuntimeError(str(p.get_editor_property('error')))
   if p.get_editor_property('physical_seconds')<=0:return
   report['reset_count']=p.get_editor_property('particle_count');report['resumed_seconds']=p.get_editor_property('physical_seconds')
   p.set_editor_property('running',False)
   parts=display.get_editor_property('parts');before=[a.static_mesh_component.get_collision_enabled() for a in actors.values() if isinstance(a,u.StaticMeshActor)]
   display.hide_equipment();assert all(not x.actor.static_mesh_component.is_visible() for x in parts)
   display.set_frames_visible(True);assert all(x.actor.static_mesh_component.is_visible()==bool(x.frame) for x in parts)
   display.show_full();assert all(x.actor.static_mesh_component.is_visible() for x in parts)
   display.show_cutaway();display.set_frames_visible(False)
   after=[a.static_mesh_component.get_collision_enabled() for a in actors.values() if isinstance(a,u.StaticMeshActor)];assert before==after
   report['display_modes_and_collision_unchanged']=True
   if '-nullrhi' in u.SystemLibrary.get_command_line().lower():finish()
   else:last=time.monotonic();state=4
  elif state==4:
   if time.monotonic()-last<4:return
   u.SystemLibrary.execute_console_command(p.get_world(),'Shot showui filename=D:/GitHub/VirtualGameTeamProj/Docs/ProcessPlant_20261110_LowPoly/preview.png')
   last=time.monotonic();state=5
  elif state==5:
   if time.monotonic()-last<3:return
   report['screenshot_requested']=True;finish()
 except Exception:finish(traceback.format_exc())
handle=u.register_slate_post_tick_callback(tick)
levels.editor_request_begin_play()


