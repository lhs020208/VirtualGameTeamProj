"""One-shot, PID-guarded revision of the live test controller, not its physics state."""
import os,gc,types,ast,inspect,json,csv,datetime,time
from pathlib import Path
import unreal as u
assert os.getpid()==17164,'Wrong process; no controls changed'
matches=[f for f in gc.get_objects() if isinstance(f,types.FunctionType) and f.__name__=='tick' and f.__code__.co_filename.replace('\\','/').endswith('/run_full_process_20261110.py')]
assert len(matches)==1
g=matches[0].__globals__;out=g['OUT'];p=g['p']
assert str(out).replace('\\','/').endswith('/FullProcess/20261110_182948')
assert g['stage']==1 and p.get_editor_property('particle_count')==20000
assert not (out/'protocol_change_applied.json').exists(),'One-shot already executed'
before=g['sample']()
rows=list(csv.DictReader((out/before['snapshot']).open(encoding='utf-8-sig')))
def in_zone(row,zone):
 x,y,z=(float(row[k]) for k in ['x_m','y_m','z_m'])
 if zone=='WH':return (x*x+y*y)**.5<.355 and .603<z<1.78
 if zone=='RH':return ((x-3)**2+y*y)**.5<.255 and 2.163<z<3.7
 return 2.6225<x<3.3775 and -.2265<y<.2265 and .941<z<1.31
ids={int(r['id']) for r in rows if in_zone(r,'WH')}
assert len(ids)==19999 and 19261 not in ids
residual=next(r for r in rows if int(r['id'])==19261)
g['cohort_ids']=ids;g['original_sample']=g['sample'];g['original_begin_stage']=g['begin_stage'];g['in_zone']=in_zone
wrapper='''
def sample_with_cohort():
 r=original_sample()
 data=list(csv.DictReader((OUT/r['snapshot']).open(encoding='utf-8-sig')))
 r['cohort_expected_count']=len(cohort_ids)
 r['cohort_counts']={zone:sum(int(row['id']) in cohort_ids and in_zone(row,zone) for row in data) for zone in ['WH','RH','MX']}
 report['latest']=r;save_report();return r
def begin_stage_with_cohort(index,sim):
 global cohort_ids
 if index in (3,4):
  data=list(csv.DictReader((OUT/report['latest']['snapshot']).open(encoding='utf-8-sig')))
  zone='RH' if index==3 else 'MX'
  cohort_ids={int(row['id']) for row in data if in_zone(row,zone)}
 (OUT/('cohort_stage_'+str(index)+'_ids.json')).write_text(json.dumps(sorted(cohort_ids)))
 original_begin_stage(index,sim)
'''
exec(compile(wrapper,'full_process_cohort_controller','exec'),g)
source=inspect.getsource(matches[0])
source=source.replace("arrived=z['WH'][stage] if stage<2 else sum(z['RH'] if stage==2 else z['MX'])","arrived=z['WH'][stage] if stage<2 else r['cohort_counts']['RH' if stage==2 else 'MX']")
source=source.replace('target=10000 if stage<2 else 20000','target=10000 if stage<2 else len(cohort_ids)')
source=source.replace("decision='completed_all_arrived'","decision='completed_all_arrived' if stage<2 else 'completed_available_cohort_arrived'")
assert 'else len(cohort_ids)' in source and "r['cohort_counts']" in source
old_tick=g['tick'];old_handle=g['handle']
u.unregister_slate_post_tick_callback(old_handle)
try:
 exec(compile(source,'full_process_revised_controller','exec'),g)
 g['sample']=g['sample_with_cohort'];g['begin_stage']=g['begin_stage_with_cohort']
 g['report']['stages'].append({**before,'result':'residual_stopped_observed_long_stagnation_criterion_not_met','start_sim_s':g['stage_sim'],'stage_wall_s':time.monotonic()-g['stage_wall'],'residual_ids':[19261]})
 change={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'pid':os.getpid(),'from_stage':1,'to_stage':2,'sim_s':before['sim_s'],'reason':'Test procedure adjustment to fulfill the full-path inspection request: inspect downstream with actual arrivals instead of waiting for an internally chosen 10s stagnation criterion','expected_cohort_count':19999,'residual':residual,'particle_count_unchanged':20000,'physics_parameters_changed':False,'reset':False,'requested_pressures_Pa_g':[0,500,-1000],'requested_supplier_mask':0,'requested_WH_open':True,'requested_RH_open':False}
 g['report']['protocol_changes']=[change]
 g['begin_stage'](2,before['sim_s'])
 (out/'protocol_change_applied.json').write_text(json.dumps(change,indent=2))
finally:
 g['handle']=u.register_slate_post_tick_callback(g.get('tick',old_tick))
