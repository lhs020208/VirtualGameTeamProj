import json,time,traceback,math
from pathlib import Path
import unreal as u
root=Path('D:/GitHub/VirtualGameTeamProj')
out=root/'Docs/ProcessPlant_20261110_LowPoly/pressure_play_validation.json'
profile=root/'Saved/PlantPressure_LowPoly20261110.json'
backup=profile.read_bytes() if profile.exists() else None
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
start=time.monotonic()
assert levels.load_level('/Game/ProcessPlant_20261110_LowPoly/L_ProcessPlant_LowPoly')
report={'level_load_wall_s':time.monotonic()-start,'cases':[],'run_logs':[]}
state=0;entered=time.monotonic();p=None
u.EditorPythonScripting.set_keep_python_script_alive(True)
def finish(error=None):
 if error:report['failure']=error
 report['total_test_wall_s']=time.monotonic()-start
 out.write_text(json.dumps(report,indent=2),encoding='utf8')
 if backup is None:profile.unlink(missing_ok=True)
 else:profile.write_bytes(backup)
 u.unregister_slate_post_tick_callback(handle)
 levels.editor_request_end_play();u.EditorPythonScripting.set_keep_python_script_alive(False);u.SystemLibrary.quit_editor()
def applied():return tuple(p.get_editor_property('applied_gauge_pa').to_tuple())
def targets():return tuple(p.get_editor_property(n) for n in ['supply_gauge_pa','weighing_gauge_pa','receiver_gauge_pa'])
def advance(name):
 global state,entered
 report['cases'].append({'case':name,'sim_s':p.get_editor_property('physical_seconds'),'wall_s':time.monotonic()-entered,'applied':applied(),'targets':targets()})
 state+=1;entered=time.monotonic()
def tick(dt):
 global p,state,entered
 try:
  if time.monotonic()-start>180:raise RuntimeError('timeout state '+str(state))
  if p and p.get_editor_property('error'):raise RuntimeError(str(p.get_editor_property('error')))
  if state==0:
   worlds=u.EditorLevelLibrary.get_pie_worlds(False)
   if not worlds:return
   matches=[a for a in u.GameplayStatics.get_all_actors_of_class(worlds[0],u.Actor) if a.get_actor_label()=='Plant_Pressure_Grains_LowPoly']
   if not matches:return
   p=matches[0]
   if not p.get_editor_property('pressure_applied'):return
   assert targets()==(0,0,0) and applied()==(0,0,0)
   assert p.get_editor_property('particle_count')==20000
   report['groups']=[c.get_instance_count() for c in p.get_components_by_class(u.InstancedStaticMeshComponent) if c.get_instance_count()]
   assert report['groups']==[10000,10000]
   report['ready_wall_s']=time.monotonic()-start;report['run_logs'].append(str(p.get_editor_property('run_log_path')))
   assert p.apply_pressure_preset(1);advance('initial_20k_zero_pressure')
  elif state==1:
   if applied()!=(500,-500,0):return
   before=targets()
   assert not p.set_pressure_targets(1001,0,0) and targets()==before
   assert not p.apply_pressure_preset(99) and targets()==before
   assert p.set_pressure_targets(700,-300,-800)
   assert p.get_editor_property('pressure_manual_override')
   assert p.save_pressure_profile()
   assert p.apply_pressure_preset(0) and p.load_pressure_profile() and targets()==(700,-300,-800)
   good=profile.read_bytes()
   for invalid in ['{bad',json.dumps({'schema':1,'supply_gauge_Pa':2000,'weighing_gauge_Pa':0,'receiver_gauge_Pa':0,'stage':0,'manual':True}),json.dumps({'schema':1,'supply_gauge_Pa':700,'weighing_gauge_Pa':0,'receiver_gauge_Pa':0,'stage':0,'manual':False})]:
    profile.write_text(invalid);assert not p.load_pressure_profile() and targets()==(700,-300,-800)
   profile.write_bytes(good)
   p.toggle_supplier(0);p.toggle_weighing_hopper();advance('preset_manual_profile_invalid_atomic')
  elif state==2:
   if applied()!=(700,-300,-800) or p.get_editor_property('transfer_flow_m3s')<=0:return
   p.toggle_receiver();advance('forward_flow')
  elif state==3:
   if applied()!=(700,-300,0):return
   p.set_editor_property('running',False);advance('receiver_open_vent')
  elif state==4:
   if time.monotonic()-entered<1:return
   assert p.set_pressure_targets(600,-200,-700);advance('pause_queue')
  elif state==5:
   if time.monotonic()-entered<1:return
   assert applied()==(700,-300,0)
   p.set_editor_property('running',True);advance('paused_applied_unchanged')
  elif state==6:
   if applied()!=(600,-200,0):return
   p.reset_simulation();assert targets()==(600,-200,-700)
   advance('reset_preserves_targets')
  elif state==7:
   if not p.get_editor_property('pressure_applied') or applied()!=(600,-200,-700):return
   assert p.get_editor_property('particle_count')==20000
   report['run_logs'].append(str(p.get_editor_property('run_log_path')))
   assert p.apply_pressure_preset(0)
   u.SystemLibrary.execute_console_command(p.get_world(),'Shot showui filename=D:/GitHub/VirtualGameTeamProj/Docs/ProcessPlant_20261110_LowPoly/pressure_preview.png')
   advance('reset_20k_closed_and_pressure_restored')
  elif state==8 and time.monotonic()-entered>3:finish()
 except Exception:finish(traceback.format_exc())
handle=u.register_slate_post_tick_callback(tick)
levels.editor_request_begin_play()
