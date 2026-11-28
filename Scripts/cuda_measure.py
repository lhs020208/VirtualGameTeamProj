"""Reproducible, serial three-stage measurements. Never controls other processes."""
import argparse
import csv
import hashlib
import json
import pathlib
import statistics
import struct
import subprocess
import threading
import time

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[1]
REPORT = ROOT / 'Docs/CUDA20261118'
DATA = ROOT / 'SourceAssets/ProcessPlant_20261111_LowPoly/Simulation'
HISTORY = ROOT / 'Docs/ProcessPlant_20261111_LowPoly/FullProcess/20261111_181306'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fixtures():
    dest = REPORT / 'fixtures'
    dest.mkdir(exist_ok=True)
    records = []
    for name, filename, phase, rpm in [
        ('transfer', '00081_WH_to_RH.csv', 1, 0),
        ('mixing', '00153_mix_15RPM.csv', 2, 15),
    ]:
        source = HISTORY / filename
        with source.open() as f:
            rows = list(csv.DictReader(f))
        assert len(rows) == 20000
        assert [int(r['id']) for r in rows] == list(range(20000))
        p = np.array([[float(r[k]) for k in ('x_m', 'y_m', 'z_m', 'species')]
                      for r in rows], dtype='<f4')
        assert np.isfinite(p).all()
        assert np.bincount(p[:, 3].astype(int)).tolist() == [10000, 10000]
        target = dest / (name + '.bin')
        target.write_bytes(struct.pack('<I', len(p)) + p.tobytes() + np.zeros_like(p).tobytes())
        records.append(dict(name=name, path=str(target.relative_to(ROOT)), sha256=sha(target),
                            source=str(source.relative_to(ROOT)), source_sha256=sha(source),
                            source_sim_s=float(rows[0]['sim_s']), phase=phase, rpm=rpm,
                            velocities='zero: historical velocities unavailable',
                            rotor_angle_rad=0,
                            interpretation='Controlled restart fixture, not a complete historical checkpoint'))
    (REPORT / 'fixtures.json').write_text(json.dumps(records, indent=2))
    return records


def telemetry(stop, result):
    query = ['nvidia-smi', '--query-gpu=timestamp,uuid,name,driver_version,pstate,utilization.gpu,utilization.memory,clocks.current.sm,clocks.current.memory,temperature.gpu,power.draw,memory.used', '--format=csv,noheader,nounits']
    while not stop.is_set():
        r = subprocess.run(query, capture_output=True, text=True, creationflags=0x08000000)
        result.append(dict(unix_s=time.time(), returncode=r.returncode, csv=r.stdout.strip()))
        stop.wait(.5)


def run(stage, scene, steps, out, dt=1e-5):
    exe = ROOT / (['Intermediate/CUDABaseline/cuda_plant_bench.exe',
                   'Intermediate/CUDAOnly/cuda_plant_bench.exe',
                   'Intermediate/CUDA/cuda_plant_bench.exe'][stage])
    fixture = 'initial' if scene['name'] == 'initial' else str(ROOT / scene['path'])
    actual_backend = 0 if stage == 0 else stage + 2
    args = [str(exe), str(actual_backend), str(DATA), fixture, str(scene['phase']), str(steps),
            str(out), str(scene['rpm']), str(dt)]
    samples = []
    stop = threading.Event()
    thread = threading.Thread(target=telemetry, args=(stop, samples))
    thread.start()
    start = time.time()
    try:
        r = subprocess.run(args, cwd=ROOT, capture_output=True, text=True, creationflags=0x08000000)
    finally:
        stop.set()
        thread.join()
    meta = dict(command=args, executable_sha256=sha(exe), start_unix_s=start,
                finish_unix_s=time.time(), stdout=r.stdout, stderr=r.stderr,
                returncode=r.returncode, telemetry=samples,
                telemetry_window='entire process, including initialization; other GPU users not stopped')
    out.with_suffix('.meta.json').write_text(json.dumps(meta, indent=2))
    print(scene['name'], stage, r.returncode, r.stdout.strip(), r.stderr.strip(), flush=True)
    if r.returncode:
        raise RuntimeError(f'Benchmark failed: {out}')
    return json.loads(out.with_suffix('.json').read_text())


def read_state(path):
    data = path.read_bytes()
    n, = struct.unpack_from('<I', data)
    a = np.frombuffer(data, dtype='<f4', offset=4).reshape(2, n, 4).astype(np.float64)
    assert n == 20000 and np.isfinite(a).all()
    return a


def analysis(folder):
    results = []
    for scene in ('initial', 'transfer', 'mixing'):
        stages = []
        states = []
        for stage in range(3):
            stems = [folder / f'{scene}_r{r}_s{stage}' for r in range(3)]
            records = [json.loads(p.with_suffix('.json').read_text()) for p in stems]
            assert len({(r['count'], r['steps'], r['dt_s'], r['phase'], r['rpm']) for r in records}) == 1
            times = [r['wall_s'] for r in records]
            med = statistics.median(times)
            stages.append(dict(stage=stage, wall_s=times, median_wall_s=med,
                               min_wall_s=min(times), max_wall_s=max(times),
                               median_rate=records[0]['sim_s']/med))
            states.append([read_state(p.with_suffix('.bin')) for p in stems])
        summary = dict(scene=scene, stages=stages,
                       cuda_time_reduction_pct=100*(1-stages[1]['median_wall_s']/stages[0]['median_wall_s']),
                       cuda_throughput_gain_pct=100*(stages[0]['median_wall_s']/stages[1]['median_wall_s']-1),
                       optimization_time_reduction_pct=100*(1-stages[2]['median_wall_s']/stages[1]['median_wall_s']),
                       optimization_throughput_gain_pct=100*(stages[1]['median_wall_s']/stages[2]['median_wall_s']-1),
                       overall_throughput_gain_pct=100*(stages[0]['median_wall_s']/stages[2]['median_wall_s']-1))
        comparisons = []
        for label, a, b in [('OpenCL repeat', states[0][0], states[0][1]),
                             ('CUDA vs OpenCL', states[0][0], states[1][0]),
                             ('graph vs CUDA', states[1][0], states[2][0])]:
            error = np.linalg.norm(a[:, :, :3]-b[:, :, :3], axis=2)
            comparisons.append(dict(label=label, position_max_m=float(error[0].max()),
                                    position_rms_m=float(np.sqrt(np.mean(error[0]**2))),
                                    velocity_max_m_s=float(error[1].max()),
                                    velocity_rms_m_s=float(np.sqrt(np.mean(error[1]**2))),
                                    group_changes=int((a[0,:,3]!=b[0,:,3]).sum()),
                                    center_of_mass_difference_m=float(np.linalg.norm(a[0,:,:3].mean(0)-b[0,:,:3].mean(0))),
                                    mean_speed_a=float(np.linalg.norm(a[1,:,:3],axis=1).mean()),
                                    mean_speed_b=float(np.linalg.norm(b[1,:,:3],axis=1).mean())))
        summary['state_comparisons'] = comparisons
        results.append(summary)
    report = dict(results=results, rendering_included=False, startup_included=False,
                  measurement='Serial physics solver wall time; equal state, count, dt, iterations, duration and hardware',
                  shared_gpu='Existing user processes left running. Raw telemetry and order are retained.',
                  physical_model='Unmodified position-based plant contacts; no experimental validation')
    (folder/'summary.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('mode', choices=['fixtures', 'preflight', 'bench', 'analyze'])
    parser.add_argument('--steps', type=int, default=3200)
    parser.add_argument('--folder', default='benchmark')
    args = parser.parse_args()
    if args.mode == 'fixtures':
        fixtures()
    elif args.mode == 'analyze':
        analysis(REPORT/args.folder)
    else:
        scenes = [dict(name='initial', phase=0, rpm=0)] + json.loads((REPORT/'fixtures.json').read_text())
        folder = REPORT / (args.folder if args.mode == 'bench' else 'preflight')
        folder.mkdir(exist_ok=True)
        for scene in scenes:
            for repeat in range(3 if args.mode == 'bench' else 1):
                for stage in [(repeat+i)%3 for i in range(3)]:
                    out = folder/f"{scene['name']}_r{repeat}_s{stage}"
                    if out.with_suffix('.json').exists():
                        raise RuntimeError(f'Refusing to overwrite measurements: {out}')
                    run(stage, scene, args.steps if args.mode == 'bench' else 16, out)
        if args.mode == 'bench':
            analysis(folder)
