"""Audit retained virtual-process evidence; never certify real material accuracy."""
from pathlib import Path
import csv, json, math, hashlib, statistics, shutil

ROOT = Path(__file__).resolve().parents[1]

def audit(root=ROOT):
    out = root / 'Docs/DEM/optimization29_36'
    manifest = json.loads((out / 'render_benchmark.json').read_text(encoding='utf-8-sig'))
    core = list(csv.DictReader((out / 'core_benchmark.csv').open(encoding='utf-8-sig')))
    assert len(core) == 27
    assert {(int(r['kg']), int(r['rpm']), int(r['repeat'])) for r in core} == {(k,p,r) for k in (10,30,50) for p in (15,30,60) for r in range(3)}
    for row in core:
        assert all(math.isfinite(float(v)) for v in row.values())
        assert float(row['wall_s']) > 0 and float(row['physical_s']) > 0
        assert math.isclose(float(row['rate']), float(row['physical_s']) / float(row['wall_s']), rel_tol=1e-7)
        assert abs(float(row['powder_error'])) < 1e-8 and abs(float(row['gas_error'])) < 1e-10
        assert 0 <= float(row['mix_index']) <= 1
        assert abs(float(row['kg']) - float(row['mixer_kg'])) < 1e-5
    runs = manifest['runs']
    assert len(runs) == 9
    assert {(int(r['kg']), r['repeat']) for r in runs} == {(k,r) for k in (10,30,50) for r in range(3)}
    for rel, digest in manifest['source_sha256'].items():
        assert hashlib.sha256((root / rel).read_bytes()).hexdigest() == digest, rel
    evidence = out / 'evidence'
    evidence.mkdir(exist_ok=True)
    for run in runs:
        assert all(math.isfinite(v) for v in run.values() if isinstance(v, (int,float)))
        assert run['complete'] and run['render_and_save_included']
        assert not run['physical_accuracy_validated']
        assert run['rate'] >= .1 and run['minimum_one_second_window_rate'] >= .1
        assert math.isclose(run['rate'], run['physical_s'] / run['real_wall_s'], rel_tol=1e-7)
        assert run['mass_error_kg'] < 1e-8 and run['frames'] > 100
        assert abs(run['mixer_kg'] - run['kg']) < 1e-5
        ref = next(r for r in core if float(r['kg']) == run['kg'] and int(r['rpm']) == 30)
        assert abs(float(ref['physical_s']) - run['physical_s']) < .02
        folder = root / 'Saved' / f"Optimization36_{int(run['kg'])}_r{run['repeat']}"
        rows = list(csv.DictReader((folder / 'trace.csv').open(encoding='utf-8-sig')))
        assert len(rows) >= run['trace_rows'] and rows[-1]['phase'] == 'complete'
        last = 0
        for row in rows:
            values = [float(v) for k,v in row.items() if k != 'phase']
            assert all(math.isfinite(v) for v in values)
            assert float(row['physical_s']) >= last
            last = float(row['physical_s'])
            assert 89999 <= float(row['pressure_pa']) <= 101326
            for name in ('total_a','total_b','total_c'):
                assert abs(float(row[name]) - run['kg']/3) < 1e-8
        state = json.loads((folder / 'state.json').read_text(encoding='utf-8-sig'))
        assert state['phase'] == 'complete' and state['restart_supported'] is False
        assert len(state['nodes']) == 53
        assert all(math.isfinite(v) and v >= 0 for node in state['nodes'] for v in node)
        for species in range(3):
            assert abs(sum(n[species] for n in state['nodes']) - run['kg']/3) < 1e-8
        dest = evidence / folder.name
        dest.mkdir(exist_ok=True)
        for name in ('trace.csv','state.json','result.json','transfer.png','mix.png'):
            assert (folder / name).stat().st_size > 0
            shutil.copy2(folder / name, dest / name)
    summary = {
        'core_cycles': len(core), 'rendered_cycles': len(runs),
        'render_rate_min': min(r['rate'] for r in runs),
        'render_rate_median': statistics.median(r['rate'] for r in runs),
        'render_window_rate_min': min(r['minimum_one_second_window_rate'] for r in runs),
        'core_rate_min': min(float(r['rate']) for r in core),
        'max_powder_error_kg': max(r['mass_error_kg'] for r in runs),
        'virtual_scenario_speed_passed': True,
        'physical_accuracy_validated': False, 'actual_plant_acceptance_passed': False,
        'source_sha256': manifest['source_sha256'],
        'evidence_sha256': {str(p.relative_to(out)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(evidence.rglob('*')) if p.is_file()}
    }
    (out / 'acceptance_audit.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in summary.items() if not k.endswith('sha256')},indent=2))
    return summary

if __name__ == '__main__':
    audit()
