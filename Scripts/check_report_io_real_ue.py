"""Short real-UE/real-Windows sharing-lock test. No saved asset changes."""
import unreal as u
import ast,csv,json,time,datetime,traceback,uuid,ctypes,os,sys
from ctypes import wintypes
from pathlib import Path
ROOT=Path('D:/GitHub/VirtualGameTeamProj');sys.path.insert(0,str(ROOT/'Scripts'))
from full_process_report_io import read_latest_report
OUT=ROOT/'Docs/ProcessPlant_20261110_LowPoly/Followup_20261110'/('UE_ReportLock_'+datetime.datetime.now().strftime('%H%M%S'));OUT.mkdir(parents=True)
report={'run_directory':str(OUT),'pid':os.getpid(),'kind':'short real UE Windows sharing-lock integration test','cases':[],'finished':False,'physical_parameters_changed':False};start=time.monotonic();state=0;entered=start;p=None;lock_handle=None
node=next(n for n in ast.parse((ROOT/'Scripts/run_full_process_20261110.py').read_text()).body if isinstance(n,ast.FunctionDef) and n.name=='save_report');exec(compile(ast.Module(body=[node],type_ignores=[]),'actual_save_report','exec'),globals())
k=ctypes.WinDLL('kernel32',use_last_error=True);k.CreateFileW.argtypes=[wintypes.LPCWSTR,wintypes.DWORD,wintypes.DWORD,wintypes.LPVOID,wintypes.DWORD,wintypes.DWORD,wintypes.HANDLE];k.CreateFileW.restype=wintypes.HANDLE;k.CloseHandle.argtypes=[wintypes.HANDLE]
def lock():
 global lock_handle
 lock_handle=k.CreateFileW(str(OUT/'status.json'),0x80000000,3,None,3,0x80,None)
 assert lock_handle not in (None,ctypes.c_void_p(-1).value),ctypes.get_last_error()
def unlock():
 global lock_handle
 if lock_handle is not None:assert k.CloseHandle(lock_handle);lock_handle=None
def sample():
 report['sim_s']=p.get_editor_property('physical_seconds');report['wall_s']=time.monotonic()-start;report['particle_count']=p.get_editor_property('particle_count');report['run_log']=str(p.get_editor_property('run_log_path'));assert report['particle_count']==20000;assert not p.get_editor_property('error')
def finish(error=None):
 if error:report['error']=error
 unlock();report['finished']=True;report['total_wall_s']=time.monotonic()-start
 (OUT/'INTEGRATION_RESULT.json').write_text(json.dumps(report,indent=2),encoding='utf8')
 u.unregister_slate_post_tick_callback(handle);levels.editor_request_end_play();u.EditorPythonScripting.set_keep_python_script_alive(False);u.SystemLibrary.quit_editor()
def tick(dt):
 global p,state,entered,first_sim,locked_sim,last_attempt
 try:
  if time.monotonic()-start>100:raise RuntimeError('short-test timeout')
  if p is None:
   worlds=u.EditorLevelLibrary.get_pie_worlds(False)
   if not worlds:return
   a=[a for a in u.GameplayStatics.get_all_actors_of_class(worlds[0],u.Actor) if a.get_actor_label()=='Plant_Pressure_Grains_LowPoly']
   if not a:return
   p=a[0]
  if not p.get_editor_property('pressure_applied'):return
  now=time.monotonic()
  if state==0:
   sample();assert save_report();first_sim=report['sim_s'];lock();entered=now;last_attempt=0;state=1
  elif state==1:
   if now-last_attempt<1:return
   sample();assert save_report() is False;latest,path=read_latest_report(OUT);assert path.name.startswith('status_recovery_') and latest['report_revision']==report['report_revision'];last_attempt=now
   if now-entered>=10:
    assert report['sim_s']>first_sim
    report['cases'].append({'case':'actual Windows no-delete-share lock while PIE runs','passed':True,'start_sim_s':first_sim,'end_sim_s':report['sim_s'],'recovery_count':len(list(OUT.glob('status_recovery_*.json'))),'latest_reader_source':path.name})
    locked_sim=report['sim_s'];unlock();entered=now;state=2
  elif state==2 and now-entered>=5:
   sample();assert report['sim_s']>locked_sim and save_report();latest,path=read_latest_report(OUT);assert path.name=='status.json' and latest['sim_s']==report['sim_s'];report['cases'].append({'case':'lock release normal publish and report reader synchronization','passed':True,'sim_s':report['sim_s']})
   lock();report['finished']=True;report['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();assert save_report() is False;latest,path=read_latest_report(OUT);assert latest['finished'] and path.name.startswith('status_recovery_');report['cases'].append({'case':'final save locked, final recovery selected','passed':True,'source':path.name,'revision':latest['report_revision']});unlock();assert save_report();latest,path=read_latest_report(OUT);assert latest['finished'] and path.name=='status.json';report['cases'].append({'case':'final normal save after unlocking','passed':True});assert save_report();finish()
 except Exception:finish(traceback.format_exc())
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);assert levels.load_level('/Game/ProcessPlant_20261110_LowPoly/L_ProcessPlant_LowPoly');u.EditorPythonScripting.set_keep_python_script_alive(True);handle=u.register_slate_post_tick_callback(tick);levels.editor_request_begin_play()
