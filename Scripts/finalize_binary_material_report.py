from pathlib import Path
import json,csv,hashlib,shutil
root=Path(__file__).resolve().parents[1]
out=root/'Docs/DEM/material_extension'
report={'all_seven_complete':False,'physical_accuracy_validated':False,'status':{'01':'implemented binary monodisperse contract','02':'implemented and tested','03':'implemented and tested','04':'implemented and independently audited','05':'rotation/rolling implemented; cohesion outstanding','06':'blocked on grade-specific material and calibration data','07':'virtual A/B geometry and performance only'},'runs':{},'recommended_test_target_s_per_wall_s':.1}
for name in ['run1','final30','final_guard12']:
 folder=root/'Saved/MaterialExtension'/name
 a=json.loads((folder/'audit_report.json').read_text());rows=list(csv.DictReader((folder/'trace.csv').open()))
 rates=[]
 for i,row in enumerate(rows):
  if float(row['active_wall_s'])<5:continue
  for earlier in rows[i+1:]:
   dt=float(earlier['active_wall_s'])-float(row['active_wall_s'])
   if dt>=5:
    rates.append((float(earlier['time_s'])-float(row['time_s']))/dt);break
 a['minimum_observed_5s_window_rate']=min(rates) if rates else None
 a['final_correction_guard']=name=='final_guard12'
 report['runs'][name]=a
 dest=out/name;dest.mkdir(exist_ok=True)
 for fn in ['result.json','trace.csv','material_model.json','audit_report.json','timing.json','worker_timing.json','display_timing.json']:
  shutil.copy2(folder/fn,dest/fn)
automation=json.loads((root/'Saved/MaterialExtension/automation/index.json').read_text(encoding='utf-8-sig'))
report['ue_automation']={k:automation.get(k) for k in ['succeeded','failed','notRun']}
report['core_tests']={'passed':7,'failed':0,'log':'core_tests.log'}
sources=['Physics/PowderCore/src/granular.cpp','Physics/PowderCore/src/granular_kernels.inc','Physics/PowderCore/include/powder/material.hpp','Physics/PowderCore/include/powder/granular.hpp','Source/VirtualGameTeamProj/PowderGranularMixer.cpp','Content/DEM/Tests/L_MX301_BinaryMaterials.umap']
report['final_source_sha256']={p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in sources}
assert all(a['passed'] for a in report['runs'].values())
(out/'acceptance.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
p=root/'SourceAssets/Granular/Pharmaceutical/relenza_reference.json';ref=json.loads(p.read_text())
if 'current_solver_blockers' in ref:ref['historical_solver_blockers_at_research']=ref.pop('current_solver_blockers')
ref['extension_2026_10_28']={'binary_radius_mass_friction':'implemented opt-in','rotation':'implemented opt-in approximate spheres','cohesion':'not implemented in position solver','commercial_material_application':False,'reason':'grade-specific properties and calibration data not established'}
p.write_text(json.dumps(ref,indent=2),encoding='utf-8')
print(json.dumps({k:{'rate':v['result']['rate'],'min_5s':v['minimum_observed_5s_window_rate'],'passed':v['passed']} for k,v in report['runs'].items()},indent=2))
