"""Exercise a powered valve through the existing GUI handlers; preserve saved charge."""
import unreal as u,time,json,traceback,csv,shutil
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve();out=root/'Docs/PoweredValves1127/play_validation.json'
out.parent.mkdir(parents=True,exist_ok=True)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);assert not levels.is_in_play_in_editor()
es=u.get_editor_subsystem(u.EditorActorSubsystem);source=next(a for a in es.get_all_level_actors() if a.get_class().get_name()=='PowderPlantTest')
original=source.get_editor_property('data_directory')
assert original=='SourceAssets/PlantRevision1125/Simulation','This geometry-specific test requires revision 1125.'
temporary=root/'Intermediate/Valve1127Data';temporary.mkdir(parents=True,exist_ok=True)
for name in ['runtime.plant','runtime.routes']:shutil.copy2(root/original/name,temporary/name)
with (temporary/'test_charge.csv').open('w',newline='') as f:
 writer=csv.writer(f);writer.writerow(['id','species','supplier','x_m','y_m','z_m','radius_m','density_kg_m3'])
 for i in range(24):
  group,j=divmod(i,12)
  writer.writerow([i,group,'H-101'+('A' if group==0 else 'B'),(-1.2 if group==0 else 0)+(j%3-1)*.0045,1.4+(j//3-1.5)*.0045,2.238,.002,1200])
source.set_editor_property('data_directory','Intermediate/Valve1127Data')
started=time.monotonic();phase=0;report={'test_particles':24};handle=None

def finish(error=None):
 global handle
 if error:report['failure']=error
 out.write_text(json.dumps(report,indent=2));source.set_editor_property('data_directory',original)
 if handle:u.unregister_slate_post_tick_callback(handle)
 levels.editor_request_end_play()
 print('POWERED_VALVE_TEST',report)

def tick(dt):
 global phase,test,ui,widget,actors,initial,opened_angle,close_time,start_seconds
 try:
  if time.monotonic()-started>180:raise RuntimeError('PIE test timeout phase '+str(phase))
  if phase==0:
   worlds=u.EditorLevelLibrary.get_pie_worlds(False)
   if not worlds:return
   world=worlds[0];actors={a.get_actor_label():a for a in u.GameplayStatics.get_all_actors_of_class(world,u.Actor)}
   test=next(a for a in actors.values() if a.get_class().get_name()=='PowderPlantTest')
   assert not str(test.get_editor_property('error')),str(test.get_editor_property('error'))
   snap=test.get_simulation_snapshot()
   if not snap.valve_actuators:return
   assert len(snap.valve_actuators)==8
   report['actuator_count']=len(snap.valve_actuators);report['backend']=str(test.get_editor_property('compute_backend'))
   assert test.get_editor_property('particle_count')==24
   assert all(v.closed for v in snap.valve_actuators)
   initial=actors['V101A_Disc'].get_actor_transform()
   sub=next((o for o in u.ObjectIterator(u.ProcessUIStateSubsystem) if o.get_outer()==world),None)
   if not sub:return
   ui=next((o for o in u.ObjectIterator() if o.get_outer()==sub and o.get_class().get_name()=='ProcessControlUI'),None)
   widget=sub.get_editor_property('status_widget')
   if not ui or not widget:return
   widget.call_method('Show2D');widget.call_method('SelectEquipment_0',('supply-a',u.Text('공급 A'),u.Text('전동 밸브 시험'),False))
   ui.call_method('StopAll');phase=1
  elif phase==1:
   if test.get_simulation_snapshot().execution!=u.PlantExecutionState.PAUSED:return
   ui.call_method('Resume');phase=2
  elif phase==2:
   if test.get_simulation_snapshot().execution!=u.PlantExecutionState.RUNNING:return
   start_seconds=test.get_simulation_snapshot().physical_seconds;ui.call_method('OpenValve');phase=3
  elif phase==3:
   snap=test.get_simulation_snapshot();assert not str(test.get_editor_property('error')),str(test.get_editor_property('error'))
   v=snap.valve_actuators[0]
   if v.angle_degrees<.5:return
   report['mid_open_degrees']=v.angle_degrees;report['motor_nm']=v.motor_torque_nm;report['watts']=v.electrical_watts
   assert not v.closed and not v.open and not v.tripped
   current=actors['V101A_Disc'].get_actor_transform();q=initial.rotation
   delta=current.rotation*u.Quat(-q.x,-q.y,-q.z,q.w)
   assert abs(delta.y)<1.e-5 and abs(delta.z)<1.e-5
   assert current.translation.distance(initial.translation)<.001
   assert abs(abs(delta.rotator().roll)-v.angle_degrees)<.1
   report['world_x_only']=True;report['disc_visible']=not actors['V101A_Disc'].get_editor_property('hidden')
   report['other_valves_closed']=all(x.closed for x in snap.valve_actuators[1:]);assert report['other_valves_closed']
   opened_angle=v.angle_degrees;ui.call_method('CloseValve');phase=4
  elif phase==4:
   snap=test.get_simulation_snapshot();assert not str(test.get_editor_property('error')),str(test.get_editor_property('error'))
   v=snap.valve_actuators[0]
   if not v.closed:return
   report['closed_after_reverse']=True;report['physical_seconds']=snap.physical_seconds;report['normal_contact_load_n']=v.normal_load_n
   report['final_particles']=test.get_editor_property('particle_count');assert report['final_particles']==24
   finish()
 except Exception:finish(traceback.format_exc())
handle=u.register_slate_post_tick_callback(tick);levels.editor_request_begin_play()
