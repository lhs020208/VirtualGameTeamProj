"""Aggregate measured results; never invent missing modes or timing values."""
import csv, json, hashlib, statistics
from pathlib import Path
root = Path(__file__).resolve().parents[1]
r = root / 'Results'
def rows(path):
    with path.open(encoding='utf-8-sig') as f:
        return list(csv.DictReader(f))
summary = {'validation': {}, 'render_invariance': {}, 'solver_performance': [], 'render_performance': []}
for name in ['basic', 'candidate', 'deposit', 'bench']:
    file = r / (name + '_checks.csv')
    if file.exists():
        data = rows(file)
        summary['validation'][name] = {'passed': sum(x['status']=='PASS' for x in data), 'failed': [x for x in data if x['status']=='FAIL']}
file = r / 'bench_performance.csv'
if file.exists():
    data = rows(file)
    for n in [200,1000,5000]:
        subset = [x for x in data if int(x['particles'])==n]
        if not subset: continue
        means = {k: statistics.mean(float(x[k]) for x in subset) for k in ['mean_ms','p95_ms','grid_ms','search_ms','contact_ms','integration_ms','stats_ms','sim_wall_ratio','contacts','candidates']}
        means.update(particles=n,repeats=len(subset),p95_note='mean of per-repeat p95 values')
        summary['solver_performance'].append(means)
render = []
for file in sorted(r.glob('*_render.csv')):
    if file.name.startswith('cold_'): continue
    data = rows(file)
    for row in data:
        row['file'] = file.name
        render.append(row)
for n in [200,1000,5000]:
    for mode in ['physics','low','high']:
        subset = [x for x in render if x['mode'].startswith(f'{mode}_{n}_')]
        if not subset: continue
        values = {k:statistics.mean(float(x[k]) for x in subset) for k in ['mean_frame_ms','p95_frame_ms','mean_prepare_ms','mean_update_ms','mean_step_ms']}
        values.update(particles=n,mode=mode,repeats=len(subset),sim_wall_ratio=.0001/(values['mean_frame_ms']/1000))
        summary['render_performance'].append(values)
    files = [f for f in sorted(r.glob(f'*_{n}_*_state.csv')) if f.name.startswith(('physics_','low_','high_'))]
    hashes = {f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
    histories = {f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(r.glob(f'*_{n}_*_history.csv')) if f.name.startswith(('physics_','low_','high_'))}
    summary['render_invariance'][str(n)] = {'files': hashes,'identical': len(set(hashes.values()))==1 if hashes else None, 'history_files': histories, 'histories_identical': len(set(histories.values()))==1 if histories else None, 'complete_9_runs': len(hashes)==9 and len(histories)==9}
(r/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
for kind in ['solver_performance','render_performance']:
    data=summary[kind]
    if data:
        with (r/(kind+'_summary.csv')).open('w',newline='') as f:
            out=csv.DictWriter(f,fieldnames=list(data[0]));out.writeheader();out.writerows(data)
print(json.dumps(summary,indent=2))
