"""Prepare reproducible test charges, not a particle transport animation.

Positions are Blender-world SI. Each supplier is discovered from the manifest;
counts, radius and density are editable there. No simulation is started.
"""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path


def charge(supplier, first_id, species):
    count = supplier['particle_count']
    r = supplier['radius_m']
    rho = supplier['density_kg_m3']
    if not isinstance(count, int) or not 1 <= count <= 1000000:
        raise ValueError('invalid count')
    if not math.isfinite(r) or r <= 0 or not math.isfinite(rho) or rho <= 0:
        raise ValueError('invalid virtual material')
    lo, hi = supplier['cylinder_bounds_m']
    center = [(lo[i] + hi[i]) / 2 for i in range(3)]
    # 20 mm margin includes render-shell versus actual inside-radius difference.
    inside = min(hi[0] - lo[0], hi[1] - lo[1]) / 2 - .02
    pitch = 2.4 * r
    side = math.ceil(count ** (1 / 3))
    if (side - 1) * pitch / math.sqrt(2) + r >= inside:
        raise ValueError('charge exceeds supplier radial clearance')
    if side * pitch + 2 * r >= hi[2] - lo[2] - .04:
        raise ValueError('charge exceeds supplier vertical clearance')
    rows = []
    for k in range(side):
        for j in range(side):
            for i in range(side):
                if len(rows) == count:
                    return rows
                p = [center[0] + (i - (side - 1) / 2) * pitch,
                     center[1] + (j - (side - 1) / 2) * pitch,
                     lo[2] + .03 + r + k * pitch]
                rows.append([first_id + len(rows), species, supplier['id'], *p, r, rho])
    return rows


def main(root):
    manifest = json.loads((root / 'manifest.json').read_text(encoding='utf-8'))
    rows, groups = [], []
    for species, supplier in enumerate(manifest['suppliers']):
        batch = charge(supplier, len(rows), species)
        rows.extend(batch)
        groups.append(dict(id=supplier['id'], species=species, count=len(batch),
                           radius_m=supplier['radius_m'],
                           mass_kg=len(batch) * 4 / 3 * math.pi * supplier['radius_m'] ** 3 * supplier['density_kg_m3']))
    dest = root / 'test_charge.csv'
    with dest.open('w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow(['id', 'species', 'supplier', 'x_m', 'y_m', 'z_m', 'radius_m', 'density_kg_m3'])
        writer.writerows(rows)
    report = dict(source_sha256=manifest['sha256'], groups=groups, total_count=len(rows),
                  charge_sha256=hashlib.sha256(dest.read_bytes()).hexdigest(),
                  simulation_executed=False, materials_calibrated=False,
                  notes=['Three colours label sources; all use the same virtual properties initially.',
                         'No teleportation, forced route, or prescribed particle trajectory. Initial state only.'])
    (root / 'test_charge_report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('root', type=Path)
    main(parser.parse_args().root)
