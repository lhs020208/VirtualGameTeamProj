import os,gc,types,json,csv,datetime,time
assert os.getpid()==17164
matches=[f for f in gc.get_objects() if isinstance(f,types.FunctionType) and f.__name__=='tick' and f.__code__.co_filename=='full_process_revised_controller']
assert len(matches)==1
g=matches[0].__globals__;out=g['OUT'];p=g['p']
assert str(out).replace('\\','/').endswith('/FullProcess/20261110_182948')
assert g['stage']==3 and p.get_editor_property('particle_count')==20000
assert not (out/'protocol_change_start_mixing.json').exists()
before=g['sample']();rows=list(csv.DictReader((out/before['snapshot']).open(encoding='utf-8-sig')))
ids=[int(r['id']) for r in rows if g['in_zone'](r,'MX')];residual=[r for r in rows if int(r['id']) not in ids]
assert 18500<len(ids)<19738
movement=json.loads((out/'RH_to_MX_terminal_residual_movement.json').read_text())
g['report']['stages'].append({**before,'result':'incomplete_terminal_inflow_procedure_transition_long_stagnation_not_met','start_sim_s':g['stage_sim'],'stage_wall_s':time.monotonic()-g['stage_wall'],'residual_ids':[int(r['id']) for r in residual],'residual_movement_evidence':movement})
change={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'pid':os.getpid(),'from_stage':3,'to_stage':4,'sim_s':before['sim_s'],'reason':'Test procedure adjustment to fulfill full-path inspection request: slow terminal inflow recorded as incomplete, inspect actual arrived particles for at least 12 physical seconds / 3 nominal rotations','expected_cohort_count':len(ids),'mixer_species_counts':before['zones']['MX'],'baseline_composition_variance':before['composition_variance'],'baseline_snapshot':before['snapshot'],'residuals':residual,'particle_count_unchanged':20000,'physics_parameters_changed':False,'reset':False,'requested_pressures_Pa_g':[0,0,0],'requested_supplier_mask':0,'requested_WH_open':False,'requested_RH_open':False,'requested_mixer_RPM':15}
g['report'].setdefault('protocol_changes',[]).append(change);g['begin_stage'](4,before['sim_s']);(out/'protocol_change_start_mixing.json').write_text(json.dumps(change,indent=2))
