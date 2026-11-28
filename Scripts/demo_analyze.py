import csv,hashlib,json,pathlib
from PIL import Image
root=pathlib.Path(__file__).resolve().parents[1];out=root/'Docs/Demo20261119'
results=[]
for i in range(3):
    label=f'demo_final_r{i}';p=out/(label+'.json')
    if not p.exists():continue
    r=json.loads(p.read_text(encoding='utf-8-sig'))
    rows=list(csv.DictReader((out/(label+'.csv')).open(encoding='utf-8-sig')))
    windows=[]
    for a,b in zip(rows,rows[10:]):
        elapsed=float(b['wall_s'])-float(a['wall_s'])
        if elapsed>0:windows.append((float(b['sim_s'])-float(a['sim_s']))/elapsed)
    r['ten_second_rate_min']=min(windows);r['ten_second_rate_max']=max(windows)
    r['max_observed_lag_s']=max(float(x['wall_s'])-float(x['sim_s']) for x in rows)
    r['snapshot_sizes']=[list(Image.open(out/f'{label}_{j}.png').size) for j in (1,2,3)]
    r['window_pass']=min(windows)>=.95 and max(windows)<=1.05
    r['resolution_pass']=all(x==[1920,1080] for x in r['snapshot_sizes'])
    r['lag_pass']=r['max_observed_lag_s']<=.1
    results.append(r)
originals=json.loads((out/'original_hashes.json').read_text())
intact=all(hashlib.sha256((root/p).read_bytes()).hexdigest()==h for p,h in originals.items())
summary=dict(trials=results,original_levels_and_inputs_preserved=intact,
    complete=intact and len(results)==3 and all(r['performance_pass'] and not r['error'] and r['window_pass'] and r['resolution_pass'] and r['lag_pass'] for r in results),
    scope='3 x 120 s, 1920x1080 output viewport, offscreen standalone game, original renderer resolution scaling (observed 1400x788), 30fps cap, camera tour; excludes startup/90 warmup frames. Own hidden child honors Unreal timer resolution. Other editor left running. Not physical accuracy certification.',
    display_particles=20000,solver_particles=2500,model='CPU soft coarse contacts + prescribed pipe coordinates + conservative rotor swept envelope and circulation field; original OpenCL/CUDA levels opt out')
(out/'acceptance_summary.json').write_text(json.dumps(summary,indent=2))
print(json.dumps(summary,indent=2))
