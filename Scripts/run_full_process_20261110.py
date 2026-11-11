"""Actual full-level PIE run. Never inject or replace particles after BeginPlay."""
import unreal as u
import csv,json,time,math,datetime,traceback,uuid
from pathlib import Path
ROOT=Path('D:/GitHub/VirtualGameTeamProj')
OUT=ROOT/'Docs/ProcessPlant_20261110_LowPoly/FullProcess'/datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
OUT.mkdir(parents=True)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
launch=time.monotonic();assert levels.load_level('/Game/ProcessPlant_20261110_LowPoly/L_ProcessPlant_LowPoly')
load_wall=time.monotonic()-launch
report={'run_directory':str(OUT),'level_load_wall_s':load_wall,'diameter_mm':4,'counts':[10000,10000],'pressure_units':'Pa(g); absolute=101325+gauge','stages':[],'completed':False}
stage=-1;p=None;request_mask=0;request_wh=False;request_rh=False;request_mix=False
last_wall=0;last_sim=-1;sequence=0;stage_sim=0;stage_wall=0;best=0;best_sim=0;complete_since=None
mix_history=[];ready_wall=None;no_progress_wall=time.monotonic();last_tick_sim=-1
names=['A_to_WH','B_to_WH','WH_to_RH','RH_to_MX','mix_15RPM']
progress=(OUT/'progress.csv').open('w',newline='',encoding='utf8')
writer=csv.writer(progress)
writer.writerow(['utc','stage','sim_s','stage_sim_s','wall_s','stage_wall_s','snapshot','A_supply','B_supply','WH_A','WH_B','RH_A','RH_B','MX_A','MX_B','other_A','other_B','missing_A','missing_B','mix_variance','pressure_supply_set_Pa_g','pressure_WH_set_Pa_g','pressure_RH_set_Pa_g','pressure_supply_applied_Pa_g','pressure_WH_applied_Pa_g','pressure_RH_applied_Pa_g','supplier_requested_mask','supplier_applied_mask','WH_requested_open','WH_applied_open','RH_requested_open','RH_applied_open','rpm'])
def save_report():
 # A reader can briefly hold status.json open on Windows. Reporting must not
 # terminate a valid physical run or throw away its in-memory solver state.
 report['report_revision']=int(report.get('report_revision',0))+1
 tmp=OUT/'status.tmp'
 try:
  tmp.write_text(json.dumps(report,indent=2),encoding='utf8');tmp.replace(OUT/'status.json')
  return True
 except PermissionError as exc:
  report.setdefault('report_io_warnings',[]).append({'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'error':str(exc),'physics_continues':True})
  recovery=OUT/('status_recovery_'+uuid.uuid4().hex+'.json')
  try:recovery.write_text(json.dumps(report,indent=2),encoding='utf8')
  except OSError as backup_error:u.log_warning('Status recovery write failed: '+str(backup_error))
  u.log_warning('Transient status write failure; simulation continues. '+str(exc))
  return False
def finish(error=None):
 if error:report['error']=error
 report['total_wall_s']=time.monotonic()-launch;report['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();save_report();progress.close()
 u.unregister_slate_post_tick_callback(handle);levels.editor_request_end_play();u.EditorPythonScripting.set_keep_python_script_alive(False);u.SystemLibrary.quit_editor()
def controls(mask,wh,rh,mix,pressures):
 global request_mask,request_wh,request_rh,request_mix
 assert p.set_pressure_targets(*pressures)
 for i in range(6):
  if bool(mask&(1<<i))!=bool(request_mask&(1<<i)):p.toggle_supplier(i)
 if wh!=request_wh:p.toggle_weighing_hopper()
 if rh!=request_rh:p.toggle_receiver()
 if mix!=request_mix:p.toggle_mixer()
 request_mask,request_wh,request_rh,request_mix=mask,wh,rh,mix
def begin_stage(index,sim):
 global stage,stage_sim,stage_wall,best,best_sim,complete_since,mix_history,last_wall
 stage=index;stage_sim=sim;stage_wall=time.monotonic();best=0;best_sim=sim;complete_since=None;mix_history=[];last_wall=0
 if index==0:controls(1,False,False,False,(500,-500,0))
 elif index==1:controls(2,False,False,False,(500,-500,0))
 elif index==2:controls(0,True,False,False,(0,500,-1000))
 elif index==3:controls(0,False,True,False,(0,0,0))
 elif index==4:controls(0,False,False,True,(0,0,0))
 report['active_stage']=names[index];save_report()
def sample():
 global sequence,last_wall,last_sim
 file=OUT/f'{sequence:05d}_{names[stage]}.csv';sequence+=1
 assert p.export_snapshot_to_file(str(file))
 rows=list(csv.DictReader(file.open(encoding='utf-8-sig')));assert len(rows)==20000
 assert len({r['id'] for r in rows})==20000
 sim=float(rows[0]['sim_s']);zones={key:[0,0] for key in ['Supply','WH','RH','MX','Other']};total=[0,0];cells={};bounds={}
 for r in rows:
  s=int(float(r['species']));assert s in (0,1);total[s]+=1
  x,y,z=(float(r[k]) for k in ['x_m','y_m','z_m']);assert all(math.isfinite(v) for v in [x,y,z])
  zone='Other'
  if 2.6<z<3.7 and 1.0<y<1.8 and -1.6<x<.4:zone='Supply'
  elif math.hypot(x,y)<.355 and .603<z<1.78:zone='WH'
  elif math.hypot(x-3,y)<.255 and 2.163<z<3.7:zone='RH'
  elif 2.6225<x<3.3775 and -.2265<y<.2265 and .941<z<1.31:zone='MX'
  zones[zone][s]+=1
  key=f'{zone}_{s}';bb=bounds.setdefault(key,[[x,y,z],[x,y,z]])
  for j,v in enumerate([x,y,z]):bb[0][j]=min(bb[0][j],v);bb[1][j]=max(bb[1][j],v)
  if zone=='MX':
   cell=(min(7,max(0,int((x-2.6225)/(.755/8)))),min(3,max(0,int((y+.2265)/(.453/4)))),min(3,max(0,int((z-.941)/(.369/4)))))
   cells.setdefault(cell,[0,0])[s]+=1
 assert total==[10000,10000],total
 n=sum(zones['MX']);variance=sum(sum(v)*(v[0]/sum(v)-.5)**2 for v in cells.values())/n if n else None
 runtime=Path(str(p.get_editor_property('run_log_path')))
 log=list(csv.DictReader(runtime.open(encoding='utf8')));actual=log[-1] if log else {}
 applied=list(p.get_editor_property('applied_gauge_pa').to_tuple());target=[p.get_editor_property(k) for k in ['supply_gauge_pa','weighing_gauge_pa','receiver_gauge_pa']]
 wall=time.monotonic();writer.writerow([datetime.datetime.now(datetime.timezone.utc).isoformat(),names[stage],sim,sim-stage_sim,wall-launch,wall-stage_wall,file.name,*zones['Supply'],*zones['WH'],*zones['RH'],*zones['MX'],*zones['Other'],10000-total[0],10000-total[1],variance,*target,*applied,request_mask,actual.get('supplier_applied_mask'),request_wh,actual.get('WH_applied_open'),request_rh,actual.get('RH_applied_open'),15 if request_mix else 0]);progress.flush()
 result={'stage':names[stage],'sim_s':sim,'stage_sim_s':sim-stage_sim,'wall_s':wall-launch,'stage_wall_s':wall-stage_wall,'snapshot':file.name,'zones':zones,'bounds_m':bounds,'composition_variance':variance,'actual_runtime_row':actual}
 report['latest']=result;report['run_log']=str(runtime);save_report();last_wall=wall;last_sim=sim
 return result
def tick(dt):
 global p,ready_wall,no_progress_wall,last_tick_sim,best,best_sim,complete_since
 try:
  if p is None:
   worlds=u.EditorLevelLibrary.get_pie_worlds(False)
   if not worlds:return
   found=[a for a in u.GameplayStatics.get_all_actors_of_class(worlds[0],u.Actor) if a.get_actor_label()=='Plant_Pressure_Grains_LowPoly']
   if not found:return
   p=found[0]
  if p.get_editor_property('error'):raise RuntimeError(str(p.get_editor_property('error')))
  if not p.get_editor_property('pressure_applied'):return
  now=time.monotonic();sim=p.get_editor_property('physical_seconds')
  if stage<0:
   ready_wall=now;report['PIE_ready_wall_s']=now-launch;assert p.get_editor_property('particle_count')==20000
   p.set_editor_property('mixer_rpm',15);begin_stage(0,sim)
  if sim!=last_tick_sim:last_tick_sim=sim;no_progress_wall=now
  elif now-no_progress_wall>180:raise RuntimeError('No physical progress for 180 wall seconds')
  if now-last_wall<30 and sim-last_sim<.5:return
  r=sample();sim=r['sim_s'];elapsed=sim-stage_sim;z=r['zones'];decision=None
  if stage<4:
   arrived=z['WH'][stage] if stage<2 else sum(z['RH'] if stage==2 else z['MX'])
   target=10000 if stage<2 else 20000
   if arrived>=target:
    if complete_since is None:complete_since=sim
    if sim-complete_since>=.5:decision='completed_all_arrived'
   else:complete_since=None
   if arrived>=best+10:best=arrived;best_sim=sim
   if elapsed>=10 and sim-best_sim>=10 and arrived<target:decision='failed_stagnation_10_physical_seconds'
   elif elapsed>=120 and arrived<target:decision='incomplete_120_physical_seconds'
  else:
   mix_history.append((sim,r['composition_variance'],z['MX']))
   if elapsed>=12:
    tail=[x for x in mix_history if x[0]>=sim-4]
    passed=all(v is not None and v<=.025 and a>=9900 and b>=9900 for _,v,(a,b) in tail)
    decision='mix_passed' if passed else ('mix_unavailable_no_arrival' if sum(z['MX'])==0 else 'mix_failed_criterion')
  if decision:
   report['stages'].append({**r,'result':decision,'start_sim_s':stage_sim,'stage_wall_s':time.monotonic()-stage_wall})
   if stage==4:
    report['completed']=all(s['result']=='completed_all_arrived' for s in report['stages'][:4]) and decision=='mix_passed';finish()
   else:begin_stage(stage+1,sim)
 except Exception:finish(traceback.format_exc())
u.EditorPythonScripting.set_keep_python_script_alive(True)
handle=u.register_slate_post_tick_callback(tick)
levels.editor_request_begin_play()
