import unreal as u, csv,json,time,traceback
from pathlib import Path
R=Path('D:/GitHub/VirtualGameTeamProj');O=R/'Docs/ProcessPlant_20261110_LowPoly/Followup_20261110/Screenshots';O.mkdir(exist_ok=True)
L=u.get_editor_subsystem(u.LevelEditorSubsystem);A=u.get_editor_subsystem(u.EditorActorSubsystem);E=u.get_editor_subsystem(u.UnrealEditorSubsystem)
assert L.load_level('/Game/ProcessPlant_20261110_LowPoly/L_ProcessPlant_LowPoly')
actors=A.get_all_level_actors(); mats={}
for a in actors:
 if 'Plant_Pressure_Grains' in a.get_actor_label():
  for i,c in enumerate(a.get_components_by_class(u.InstancedStaticMeshComponent)):
   if c.get_instance_count():mats[len(mats)]=c.get_material(0)
  A.destroy_actor(a)
D=next(a for a in actors if a.get_actor_label()=='Plant_DisplayControls');D.show_cutaway();D.set_frames_visible(False)
sphere=u.load_asset('/Engine/BasicShapes/Sphere');particles=[];bounds={}
for group in ['B','WH','RH']:
 rows=list(csv.DictReader((O.parent/(group+'_residual_input.csv')).open()));ps=[]
 for row in rows:
  p=[float(row[k]) for k in ['x_m','y_m','z_m']];ps.append(p)
  a=A.spawn_actor_from_class(u.StaticMeshActor,u.Vector(p[0]*100,-p[1]*100,p[2]*100));a.set_actor_label('Recorded_'+group+'_'+row['id']);a.static_mesh_component.set_static_mesh(sphere);a.set_actor_scale3d(u.Vector(.004,.004,.004));a.static_mesh_component.set_material(0,mats[int(row['species'])]);particles.append(a)
 bounds[group]={'count':len(rows),'min_m':[min(p[k] for p in ps) for k in range(3)],'max_m':[max(p[k] for p in ps) for k in range(3)]}
# Original SI coordinates converted by x*100,-y*100,z*100 exactly as runtime.
for a in A.get_all_level_actors():
 if a.get_actor_label().startswith('P101B_'):a.set_is_temporarily_hidden_in_editor(True)
shots=[('08_B_exposed',(37,-125,209),(30.62164,-140.00750,202.15065))]

report={'notice':'Recorded coordinate reconstruction, NOT a live simulation','sim_s':38.2652790333,'particle_diameter_m':.004,'groups':bounds,'display':'existing render cutaway materials; frame hidden; recorded particles only; original render meshes','shots':[]};n=0;t=time.monotonic();phase=0;busy=False
u.EditorPythonScripting.set_keep_python_script_alive(True)
def tick(dt):
 global n,t,phase,busy
 if busy:return
 busy=True
 try:
  if time.monotonic()-t<3:return
  if n==len(shots):
   (O/'capture_B_exposed_manifest.json').write_text(json.dumps(report,indent=2));u.unregister_slate_post_tick_callback(handle);u.EditorPythonScripting.set_keep_python_script_alive(False);u.SystemLibrary.quit_editor();return
  name,eye,target=shots[n]
  if phase==0:
   E.set_level_viewport_camera_info(u.Vector(*eye),u.MathLibrary.find_look_at_rotation(u.Vector(*eye),u.Vector(*target)));phase=1;t=time.monotonic()
  else:
   u.AutomationLibrary.take_high_res_screenshot(1600,1000,str(O/(name+'_raw.png')))
   report['shots'].append({'name':name,'camera_cm':eye,'target_cm':target});n+=1;phase=0;t=time.monotonic()
 except Exception:
  (O/'ERROR.txt').write_text(traceback.format_exc());u.unregister_slate_post_tick_callback(handle);u.EditorPythonScripting.set_keep_python_script_alive(False);u.SystemLibrary.quit_editor()
 finally:busy=False
handle=u.register_slate_post_tick_callback(tick)
