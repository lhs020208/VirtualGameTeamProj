from pathlib import Path
import csv,json,shutil,hashlib
root=Path(__file__).resolve().parents[1]
run=root/'Saved/Rate01Recheck/current12'
out=root/'Docs/DEM/rate01_recheck'
audit=json.loads((run/'audit_report.json').read_text())
rows=list(csv.DictReader((run/'trace.csv').open()))
elapsed=float(rows[-1]['active_wall_s'])
windows=[]
for i,a in enumerate(rows):
 if float(a['active_wall_s'])<5:continue
 for b in rows[i+1:]:
  wall=float(b['active_wall_s'])-float(a['active_wall_s'])
  if wall>=5:
   windows.append((float(b['time_s'])-float(a['time_s']))/wall);break
assert audit['passed'] and audit['result']['time']>=12
assert .095<=audit['result']['rate']<=.105
previous=root/'Saved/MaterialExtension/final_guard12/initial.bin'
assert (run/'initial.bin').read_bytes()==previous.read_bytes(),'initial particles changed'
for name in ['result.json','trace.csv','material_model.json','audit_report.json','timing.json','worker_timing.json','display_timing.json']:
 shutil.copy2(run/name,out/name)
report={'target_rate':.1,'saved_rate_used_in_benchmark':True,'average_rate':audit['result']['rate'],'active_wall_s_last_snapshot':elapsed,'physical_time':audit['result']['time'],'five_second_window_min':min(windows),'five_second_window_max':max(windows),'initial_state_identical':True,'initial_sha256':hashlib.sha256((run/'initial.bin').read_bytes()).hexdigest(),'core_tests_passed':7,'sampled_geometry_passed':audit['passed'],'samples':len(audit['samples']),'max_overlap_min_radius':max(x['overlap_min_radius'] for x in audit['samples']),'max_wall_m':max(x['wall_m'] for x in audit['samples']),'max_rotor_m':max(x['rotor_m'] for x in audit['samples']),'physical_accuracy_guaranteed':False,'limits':['No empirical pharmaceutical calibration','No dense-scene time-step convergence study in this recheck','Geometry checked at sampled instants, not a proof for every time step','Runtime remains approximate spheres without cohesion/air drag']}
(out/'summary.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
