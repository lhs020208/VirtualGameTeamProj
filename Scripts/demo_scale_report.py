import csv,hashlib,json,pathlib,re,subprocess
root=pathlib.Path(__file__).resolve().parents[1];out=root/'Docs/DemoScale200k'
def read(name):return json.loads((out/name).read_text(encoding='utf-8-sig'))
def analyze(label):
 r=read(label+'.json');rows=list(csv.DictReader((out/(label+'.csv')).open(encoding='utf-8-sig')))
 r['label']=label;r['wall_seconds_to_sim_60']=None
 for a,b in zip(rows,rows[1:]):
  s0,s1=float(a['sim_s']),float(b['sim_s']);w0,w1=float(a['wall_s']),float(b['wall_s'])
  if s0<=60<=s1 and s1>s0:r['wall_seconds_to_sim_60']=w0+(60-s0)/(s1-s0)*(w1-w0);break
 a,b=rows[-31],rows[-1]
 rate=(float(b['sim_s'])-float(a['sim_s']))/(float(b['wall_s'])-float(a['wall_s']))
 r['last_30_wall_seconds_rate']=rate;r['estimated_wall_seconds_per_minute_in_late_stage']=60/rate
 r['max_sampled_sim_lag_s']=max(float(x['wall_s'])-float(x['sim_s']) for x in rows)
 return r
results=[analyze(x) for x in ['baseline_r0','scaled_r0','visual_r0','visual_incremental_r0']]
base,scaled,visual,inc=results
before=read('protected_hashes.json')
unchanged={p:hashlib.sha256((root/p).read_bytes()).hexdigest()==h for p,h in before.items()}
assert all(unchanged.values()),'protected project changed; inspect before reporting'
serial=read('reference_hash.json');parallel=read('parallel.json');validation=read('parallel_validation.json')
parallel_at_50=None
for line in (out/'parallel.progress.txt').read_text().splitlines():
 fields=dict(x.split('=',1) for x in line.split())
 if 50<=float(fields['sim'])<50.01:parallel_at_50=float(fields['wall'])
assert parallel_at_50 and validation['bitwise_identical_at_50s']
report=dict(date='2026-11-19',question='200,000 displayed particles: elapsed simulation time and optimization headroom',
 hardware=dict(cpu='i7-14700KF',gpu='RTX 4060 Ti 16 GB',driver='610.88',engine='UE 5.8'),
 conditions=dict(output_resolution=[1920,1080],internal_resolution_observed=[1400,788],wall_seconds_per_rendered_test=150,cap_fps=30,time_step_s=1/120,contact_iterations=1,simultaneous_benchmarks=False,original_editor_left_running=True,offscreen_timer_request_honored=True),
 interpretations=dict(scaled='200k display / 25k compute; unchanged 2mm radius and 1200 kg/m3 density -> nominal 8.04248kg, ten times material.',visual='200k display / 2500 compute; each original visual grain subdivided into ten smaller visual grains (0.928318mm radius). Original physical quantity and 20k input grains remain unchanged.',prototype_ui='Test harness retains the old static UI header; counts in JSON are instrumented instance counts, not that label.'),
 rendered=results,native_serial=read('core_200k.json'),native_baseline=read('core_20k.json'),
 incremental_renderer=dict(change='BatchUpdateInstancesTransforms bMarkRenderStateDirty=false. UE 5.8 TransformChanged already requests instance-data updates; avoid recreating entire scene proxies.',
   physics_changed=False,display_count_changed=False,fps_gain_percent=(inc['fps']/visual['fps']-1)*100,frame_p95_reduction_percent=(1-inc['p95_ms']/visual['p95_ms'])*100,
   source='UE_5.8/Engine/Source/Runtime/Engine/Private/InstancedStaticMesh/ISMInstanceDataManager.cpp:TransformChanged and MarkComponentRenderInstancesDirty; InstancedStaticMesh.cpp:BatchUpdateInstancesTransformsInternal'),
 parallel_contact_experiment=dict(threads=12,result=parallel,reference_50s=serial,validation=validation,
   matched_simulation_interval_s=[0,50],serial_wall_s=serial['wall_s'],parallel_wall_s=parallel_at_50,
   speedup=serial['wall_s']/parallel_at_50,elapsed_reduction_percent=(1-parallel_at_50/serial['wall_s'])*100,
   limitation='Standalone solver experiment, not whole-application fps. Timings compare the same 0-50 s simulation interval, with bitwise-identical positions at its end. The 90 s candidate additionally measures steady mixing. Combined rendered 200k/25k + parallel physics + incremental rendering has not been tested.'),
 original_project_unchanged=unchanged,installed_changes=False,commits_created=False,new_worktrees=False,
 limits=['One 150s rendered trial per condition, not a statistical guarantee','A displayed frame can advance less than one frame of physical time; FPS and simulation rate must be read separately','Native solver throughput excludes rendering and wall-clock pacing','20k -> 200k same-size particles changes both count and material mass','Visual subdivision increases apparent detail without increasing independent physical degrees of freedom'])
(out/'RESULTS.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
for r in results:print(r['label'],r['fps'],r['rate'],'wall_to_sim60=',r['wall_seconds_to_sim_60'],'late_rate=',r['last_30_wall_seconds_rate'])
print('incremental FPS gain:',report['incremental_renderer']['fps_gain_percent'])
print('parallel mixing step ms:',parallel['phases'][3]['step_mean_ms'])
print('protected files unchanged:',all(unchanged.values()))
