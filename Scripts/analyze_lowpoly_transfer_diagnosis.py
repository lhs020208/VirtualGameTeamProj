"""Read worker-owned diagnostic captures; never run or modify the solver.

Usage: python Scripts/analyze_lowpoly_transfer_diagnosis.py <capture_directory>
Outputs analysis.json, inventory.csv and late_particle_history.csv beside captures.
All coordinates are solver metres; all masses are model kg. No third-party packages.
"""
import argparse
import csv
import json
import math
import struct
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / 'SourceAssets/ProcessPlant_20261111_LowPoly/Simulation'
SEGMENTS = [(60, 'outlet_lower_bends'), (71, 'narrow_riser'),
            (74, 'expansion'), (181, 'wide_riser'), (245, 'upper_bend'),
            (256, 'horizontal_before_BV122'), (257, 'BV122'),
            (264, 'receiver_inlet')]


def vector(values, particle):
    return tuple(values[4 * particle:4 * particle + 3])


def norm(v):
    return math.sqrt(sum(x*x for x in v))


def stats(values):
    a = sorted(values)
    if not a:
        return {'n': 0}
    return {'n': len(a), 'min': a[0], 'median': a[len(a)//2],
            'p90': a[min(len(a)-1, int(.9*len(a)))], 'max': a[-1],
            'mean': sum(a)/len(a)}


def read_frame(path):
    meta = json.loads(path.read_text())
    raw = path.with_suffix('.bin').read_bytes()
    n, = struct.unpack_from('<I', raw)
    assert n == meta['count'] and len(raw) == 4 + 49*n, path
    # Windows and the supported analysis hosts are little endian.
    assert struct.pack('=I', 1) == struct.pack('<I', 1)
    arrays = [memoryview(raw)[4+j*16*n:4+(j+1)*16*n].cast('f') for j in range(3)]
    regions = raw[4+48*n:]
    assert all(r in range(5) for r in regions)
    assert all(math.isfinite(v) for a in arrays for v in a)
    return {'path': path.name, 'm': meta, 'p': arrays[0], 'v': arrays[1],
            'air': arrays[2], 'r': regions}


def read_routes():
    raw = (DATA/'runtime.plant').read_bytes()
    row = lambda i: struct.unpack_from('<4f', raw, 4+16*i)
    routes = []
    for line in (DATA/'runtime.routes').read_text().splitlines()[1:]:
        phase, count, *indices = map(int, line.split())
        assert count == len(indices)
        cells = []
        arc = 0.
        for index in indices:
            a, b = row(index), row(index+1)
            d = tuple(b[k]-a[k] for k in range(3))
            length2 = sum(x*x for x in d)
            cells.append((a, b, d, length2, arc))
            arc += math.sqrt(length2)
        routes.append({'phase': phase, 'cells': cells, 'length_m': arc})
    gates = [row(int(row(5)[0])+2*j) for j in range(int(row(5)[1]))]
    return routes, gates


def nearest(p, cells):
    best = None
    for i, (a, b, d, length2, arc) in enumerate(cells):
        t = max(0., min(1., sum((p[k]-a[k])*d[k] for k in range(3))/length2))
        radial2 = sum((p[k]-a[k]-t*d[k])**2 for k in range(3))
        radius = a[3]+t*(b[3]-a[3])
        metric = radial2/radius**2
        if best is None or metric < best['normalized_radius2']:
            best = {'cell': i, 'arc_m': arc+t*math.sqrt(length2),
                    'normalized_radius2': metric}
    return best


def segment(p, cells):
    # Fast coarse bounds discard distant cells, retaining the actual route geometry.
    candidates = []
    for i, cell in enumerate(cells[13:], 13):
        a, b, d, length2, arc = cell
        radius = max(a[3], b[3])
        if all(min(a[k], b[k])-radius-1e-6 <= p[k] <= max(a[k], b[k])+radius+1e-6 for k in range(3)):
            candidates.append((i, cell))
    if not candidates:
        hit = nearest(p, cells)
    else:
        hit = nearest(p, [c for i, c in candidates])
        hit['cell'] = candidates[hit['cell']][0]
    hit['segment'] = next(name for end, name in SEGMENTS if hit['cell'] <= end)
    return hit


def receiver_geometry(p):
    x, y, z = p
    r2 = (x-3)**2+y*y
    if not 2.1600000858306885 <= z < float32(3.39):
        return False
    if (float32(3.037) <= z < float32(3.047) and r2 <= .095**2 or
        float32(3.047) <= z < float32(3.387) and r2 <= .06**2 or
        float32(3.387) <= z < float32(3.39) and r2 <= .10**2):
        return False
    radius = (.136 if z < 2.292 else .136+(z-2.292)/.015*.005 if z < 2.307
              else .141+(z-2.307)/.18*.105 if z < 2.487 else .246)
    spigot = x < float32(2.77) and y*y+(z-float32(2.682))**2 <= float32(.061)**2
    return r2 <= radius**2 and not spigot


def float32(v):
    return struct.unpack('<f', struct.pack('<f', v))[0]


def main(directory):
    paths = sorted(directory.glob('[0-9]*_*.json'))
    frames = [read_frame(p) for p in paths]
    assert frames
    routes, gates = read_routes()
    cells = next(r['cells'] for r in routes if r['phase'] == 1)
    mass = 4*math.pi*frames[0]['m']['radius_m']**3*frames[0]['m']['density_kg_m3']/3
    openings = [f for f in frames if f['m']['tag'] == 'before_open' and f['m']['baseline_kg'] > 0]
    opening = openings[0] if openings else None
    cohort = {i for i, r in enumerate(opening['r']) if r == 1} if opening else set()
    start = opening['m']['simulation_s'] if opening else math.inf
    rows, errors, late = [], [], set()
    for f in frames:
        m = f['m']; counts = Counter(f['r']); own = Counter(f['r'][i] for i in cohort)
        if m['tag'] in ('sample', 'pause', 'before_open'):
            for r, key in ((1, 'wh_kg'), (2, 'pipe_kg'), (3, 'rh_kg')):
                if abs(counts[r]*mass-m[key]) > 1e-12:
                    errors.append({'file': f['path'], 'field': key, 'error_kg': counts[r]*mass-m[key]})
            if abs(m['telemetry_s']-m['simulation_s']) > 1e-12:
                errors.append({'file': f['path'], 'field': 'telemetry_time'})
        if m['simulation_s'] > start:
            late.update(i for i, r in enumerate(f['r']) if i not in cohort and r in (1, 2, 3) and opening['r'][i] != 3)
        row = {'file': f['path'], 'simulation_s': m['simulation_s'], 'wall_s': m['wall_s'],
               'tag': m['tag'], 'requested_supplier_mask': m['requested_supplier_mask'],
               'applied_supplier_mask': m['applied_supplier_mask'], 'applied_wh': m['applied_wh'],
               'all_wh_kg': counts[1]*mass, 'all_pipe_kg': counts[2]*mass, 'all_rh_kg': counts[3]*mass,
               'cohort_wh_kg': own[1]*mass, 'cohort_pipe_kg': own[2]*mass, 'cohort_rh_kg': own[3]*mass,
               'cohort_outside_kg': own[0]*mass, 'late_ids_seen': len(late)}
        rows.append(row)
    with (directory/'inventory.csv').open('w', newline='') as stream:
        w = csv.DictWriter(stream, fieldnames=list(rows[0])); w.writeheader(); w.writerows(rows)
    report = {'capture': str(directory.relative_to(ROOT)), 'frames': len(frames),
              'particle_mass_kg': mass, 'baseline_count': len(cohort), 'baseline_kg': len(cohort)*mass,
              'mass_or_freshness_errors': errors, 'final_inventory': rows[-1],
              'segment_definition': [{'name': name, 'last_route_cell': end} for end, name in SEGMENTS],
              'scope': 'This new bounded run is not a recovered trajectory of the previous run.'}
    if opening:
        applied = next((f for f in frames if f['m']['simulation_s'] == start and f['m']['tag'] == 'control_applied' and f['m']['applied_wh']), None)
        report['baseline_application'] = {'pre_open': opening['m'], 'applied': applied['m'] if applied else None,
            'positions_identical': bool(applied and opening['p'].tobytes() == applied['p'].tobytes())}
    active = [f for f in frames if f['m']['simulation_s'] > start and f['m']['applied_wh'] and f['m']['requested_wh'] and f['m']['applied_supplier_mask'] == 32 and f['m']['requested_supplier_mask'] == 32 and f['m']['tag'] == 'sample']
    if active:
        f = active[-1]; m = f['m']; groups = {name: [] for end, name in SEGMENTS}
        for i, r in enumerate(f['r']):
            if r == 2:
                groups[segment(vector(f['p'], i), cells)['segment']].append(i)
        segments = []
        prior = min(active, key=lambda a: abs(a['m']['simulation_s']-(m['simulation_s']-.5)))
        elapsed = m['simulation_s']-prior['m']['simulation_s']
        representatives = []
        for name, ids in groups.items():
            points = [vector(f['p'], i) for i in ids]
            speed = [norm(vector(f['v'], i)) for i in ids]
            air = [norm(vector(f['air'], i)) for i in ids]
            delta = [math.dist(vector(f['p'], i), vector(prior['p'], i)) for i in ids]
            s = {'segment': name, 'particles': len(ids), 'kg': mass*len(ids),
                 'cohort_particles': sum(i in cohort for i in ids),
                 'speed_m_s': stats(speed), 'air_m_s': stats(air), 'zero_air_count': sum(a < 1e-12 for a in air),
                 'displacement_m': stats(delta), 'displacement_time_s': elapsed,
                 'unchanged_position_count': sum(d == 0 for d in delta),
                 'displacement_under_0_1mm_count': sum(d < .0001 for d in delta)}
            if points:
                s['xyz_min_m'] = [min(p[k] for p in points) for k in range(3)]
                s['xyz_max_m'] = [max(p[k] for p in points) for k in range(3)]
                # Stable-ID representative near the coordinate median, plus the ends.
                ordered = sorted(ids, key=lambda i: vector(f['p'], i))
                for i in sorted({ordered[0], ordered[len(ordered)//2], ordered[-1]}):
                    representatives.append({'id': i, 'segment': name, 'cohort': i in cohort,
                        'position_m': vector(f['p'], i), 'velocity_m_s': vector(f['v'], i),
                        'air_m_s': vector(f['air'], i), 'previous_position_m': vector(prior['p'], i),
                        'previous_simulation_s': prior['m']['simulation_s']})
            segments.append(s)
        report['last_flowing_sample'] = m
        report['motion_comparison_from_s'] = prior['m']['simulation_s']
        report['segments'] = segments; report['representatives'] = representatives
        pipe_ids = [i for i, r in enumerate(f['r']) if r == 2]
        report['motion_windows'] = []
        for window in (.5, .25, .1):
            previous = min(active, key=lambda q: abs(q['m']['simulation_s']-m['simulation_s']+window))
            distances = [math.dist(vector(f['p'], i), vector(previous['p'], i)) for i in pipe_ids]
            report['motion_windows'].append({'from_simulation_s': previous['m']['simulation_s'],
                'to_simulation_s': m['simulation_s'], 'pipe_count': len(pipe_ids),
                'displacement_m': stats(distances), 'exactly_unchanged_count': sum(d == 0 for d in distances),
                'under_0_1mm_count': sum(d < .0001 for d in distances),
                'over_1mm_count': sum(d > .001 for d in distances)})
        report['pipe_instantaneous_speed_m_s'] = stats([norm(vector(f['v'], i)) for i in pipe_ids])
        report['pipe_points_inside_receiver_geometry'] = [i for i, r in enumerate(f['r']) if r == 2 and receiver_geometry(vector(f['p'], i))]
        report['receiver_points_outside_receiver_geometry'] = [i for i, r in enumerate(f['r']) if r == 3 and not receiver_geometry(vector(f['p'], i))]
        report['cohort_receiver_positions_m'] = [vector(f['p'], i) for i in cohort if f['r'][i] == 3]
        timeline = []
        chosen = sorted({min(range(len(active)), key=lambda i: abs(active[i]['m']['simulation_s']-t))
                         for t in (start+.25, start+.5, start+1., start+1.5,
                                   m['simulation_s']-.5, m['simulation_s']-.25, m['simulation_s'])})
        for index in chosen:
            sample = active[index]; parts = Counter(); part_cohort = Counter(); speeds = {}
            for i, region in enumerate(sample['r']):
                if region != 2:
                    continue
                name = segment(vector(sample['p'], i), cells)['segment']
                parts[name] += 1; part_cohort[name] += i in cohort
                speeds.setdefault(name, []).append(norm(vector(sample['v'], i)))
            for end, name in SEGMENTS:
                timeline.append({'simulation_s': sample['m']['simulation_s'],
                    'elapsed_after_open_s': sample['m']['simulation_s']-start,
                    'segment': name, 'count': parts[name], 'kg': parts[name]*mass,
                    'cohort_count': part_cohort[name], 'cohort_kg': part_cohort[name]*mass,
                    'median_speed_m_s': stats(speeds.get(name, [])).get('median')})
        report['segment_timeline'] = timeline
        with (directory/'segments.csv').open('w', newline='') as stream:
            w = csv.DictWriter(stream, fieldnames=list(timeline[0])); w.writeheader(); w.writerows(timeline)
    charges = list(csv.DictReader((DATA/'test_charge.csv').open()))
    report['late_particles'] = [{'id': i, 'original_supplier': charges[i]['supplier'],
        'opening_position_m': vector(opening['p'], i), 'opening_region': opening['r'][i],
        'opening_nearest_supply_route': min(((nearest(vector(opening['p'], i), r['cells'])['normalized_radius2'], j) for j, r in enumerate(routes) if r['phase'] == 0))} for i in sorted(late)]
    with (directory/'late_particle_history.csv').open('w', newline='') as stream:
        w = csv.writer(stream); w.writerow(['id', 'simulation_s', 'tag', 'region', 'x', 'y', 'z', 'vx', 'vy', 'vz', 'air_x', 'air_y', 'air_z', 'requested_suppliers', 'applied_suppliers', 'applied_wh'])
        for i in sorted(late):
            for f in frames:
                m = f['m']; w.writerow([i, m['simulation_s'], m['tag'], f['r'][i], *vector(f['p'], i), *vector(f['v'], i), *vector(f['air'], i), m['requested_supplier_mask'], m['applied_supplier_mask'], m['applied_wh']])
    report['gates_m'] = gates
    (directory/'analysis.json').write_text(json.dumps(report, ensure_ascii=False, indent=2))
    print(json.dumps({k: report[k] for k in ('frames', 'baseline_count', 'baseline_kg', 'mass_or_freshness_errors', 'final_inventory')}, indent=2))
    if 'segments' in report:
        for s in report['segments']:
            print(s['segment'], s['particles'], s['kg'], s['displacement_m'], s['air_m_s'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture_directory', type=Path)
    args = parser.parse_args()
    main(args.capture_directory.resolve())
