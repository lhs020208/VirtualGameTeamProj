"""Short PIE check through the actual control UI's start/stop handlers."""
import unreal as u,time,json,traceback,math
from pathlib import Path
out=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir()))/'Docs/PlantRevision1125/play_validation.json'
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
assert not levels.is_in_play_in_editor()
started=time.monotonic();state=0;report={};handle=None
def finish(error=None):
 if error:report['failure']=error
 out.write_text(json.dumps(report,indent=2))
 u.unregister_slate_post_tick_callback(handle);levels.editor_request_end_play()
def tick(dt):
 global state,actors,test,ui,initial,static_initial,stopped_angle,stop_time,sub
 try:
  if time.monotonic()-started>120:raise RuntimeError('PIE timeout, state '+str(state))
  if state==0:
   worlds=u.EditorLevelLibrary.get_pie_worlds(False)
   if not worlds:return
   world=worlds[0];actors={a.get_actor_label():a for a in u.GameplayStatics.get_all_actors_of_class(world,u.Actor)}
   test=actors['Plant_Pressure_Grains_LowPoly']
   error=str(test.get_editor_property('error'));assert not error,error
   if test.get_editor_property('physical_seconds')<=0:return
   sub=next((s for s in u.ObjectIterator(u.ProcessUIStateSubsystem) if s.get_outer()==world),None)
   if not sub:return
   # The control helper has reflected UFUNCTIONs but is not a Blueprint/Python
   # class. Invoke the same reflected handlers used by the widget's buttons.
   ui=next((o for o in u.ObjectIterator() if o.get_outer()==sub and o.get_class().get_name()=='ProcessControlUI'),None)
   widget=sub.get_editor_property('status_widget')
   if not ui or not widget:return
   widget.call_method('Show2D')
   widget.call_method('SelectEquipment_0',('mixer',u.Text('혼합기 MX-301'),u.Text('회전 확인'),True))
   initial={n:a.get_actor_transform() for n,a in actors.items() if a.actor_has_tag('PlantMixerRotor')}
   static_initial={n:a.get_actor_transform() for n,a in actors.items() if a.actor_has_tag('PlantMixerStatic')}
   assert len(initial)==47
   report['backend']=str(test.get_editor_property('compute_backend'))
   assert test.get_editor_property('compute_backend')==u.PlantComputeBackend.OPEN_CL
   report['particles']=test.get_editor_property('particle_count');assert report['particles']==20000
   report['groups']=[c.get_instance_count() for c in test.get_components_by_class(u.InstancedStaticMeshComponent) if c.get_instance_count()]
   assert report['groups']==[10000,10000]
   # The current control screen admits manual commands only after a verified
   # whole stop, followed by explicit resume. Exercise that normal operator path.
   ui.call_method('StopAll');state=10
  elif state==10:
   if test.get_simulation_snapshot().execution!=u.PlantExecutionState.PAUSED:return
   ui.call_method('Resume');state=11
  elif state==11:
   if test.get_simulation_snapshot().execution!=u.PlantExecutionState.RUNNING:return
   ui.call_method('StartMixer');state=1
  elif state==1:
   error=str(test.get_editor_property('error'));assert not error,error
   snap=test.get_simulation_snapshot()
   if not snap.applied_rpm_valid or snap.applied_rpm<=0:return
   q=initial['MX-301_Paddle_01'].rotation
   rotation=actors['MX-301_Paddle_01'].get_actor_transform().rotation*u.Quat(-q.x,-q.y,-q.z,q.w)
   angle=rotation.rotator().roll
   if abs(angle)<.05:return
   # A single rigid transform, around the physics pivot, must move every rotor
   # component including the couplings and sensor target. Static seals must stay put.
   assert abs(rotation.y)<1e-5 and abs(rotation.z)<1e-5
   pivot=u.Vector(300,0,90);errors=[]
   for n,t in initial.items():
    expected=pivot+rotation.rotate_vector(t.translation-pivot)
    errors.append(actors[n].get_actor_location().distance(expected))
   assert max(errors)<.02,max(errors)
   assert all(actors[n].get_actor_transform()==t for n,t in static_initial.items())
   report['rotating_parts']=len(initial);report['static_parts_unchanged']=len(static_initial)
   report['rotation_degrees']=angle;report['max_rigid_transform_error_cm']=max(errors)
   report['applied_rpm']=snap.applied_rpm
   ui.call_method('StopMixer');state=2
  elif state==2:
   snap=test.get_simulation_snapshot()
   if not snap.applied_rpm_valid or snap.applied_rpm!=0:return
   stopped_angle=actors['MX-301_Paddle_01'].get_actor_rotation().roll;stop_time=test.get_editor_property('physical_seconds');state=3
  elif state==3:
   if test.get_editor_property('physical_seconds')<=stop_time+.001:return
   assert abs(actors['MX-301_Paddle_01'].get_actor_rotation().roll-stopped_angle)<1e-5
   assert not str(test.get_editor_property('error'))
   report['motor_stopped_while_particles_advance']=True
   report['physical_seconds']=test.get_editor_property('physical_seconds')
   report['device']=str(test.get_editor_property('compute_device'))
   finish()
 except Exception:finish(traceback.format_exc())
handle=u.register_slate_post_tick_callback(tick)
levels.editor_request_begin_play()
