import os,gc,types,csv,json,datetime
assert os.getpid()==17164
f=[f for f in gc.get_objects() if isinstance(f,types.FunctionType) and f.__name__=='tick' and f.__code__.co_filename=='full_process_revised_controller'];assert len(f)==1
g=f[0].__globals__;assert g['stage']==4
out=g['OUT'];assert not (out/'mix_applied_start.json').exists()
change=json.loads((out/'protocol_change_start_mixing.json').read_text());rows=list(csv.DictReader(open(g['report']['run_log'],encoding='utf8')))
row=next(r for r in rows if float(r['sim_s'])>=change['sim_s'] and r['event']=='control_applied' and float(r['mixer_RPM'])==15 and r['RH_applied_open']=='0')
actual=float(row['sim_s']);assert actual>=g['stage_sim'] and actual-g['stage_sim']<.1
old=g['stage_sim'];g['stage_sim']=actual
g['report']['mix_applied_start']={'request_sim_s':old,'applied_sim_s':actual,'minimum_end_sim_s':actual+12,'actual_runtime_row':row,'note':'Observation duration starts at confirmed applied RPM=15 and receiver closed; physical simulation unchanged'}
g['save_report']();(out/'mix_applied_start.json').write_text(json.dumps(g['report']['mix_applied_start'],indent=2))
