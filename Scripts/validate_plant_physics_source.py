"""Blender background geometry preflight; does not run particle or gas dynamics."""
import csv
import hashlib
import json
import math
import sys
from collections import Counter
from pathlib import Path

from mathutils import Vector
from mathutils.bvhtree import BVHTree


root = Path(sys.argv[sys.argv.index('--') + 1])
manifest = json.loads((root / 'manifest.json').read_text(encoding='utf-8'))
assert hashlib.sha256(Path(manifest['source']).read_bytes()).hexdigest() == manifest['sha256']
meshes = {m['id']: m for m in manifest['meshes']}


def tree(name):
    m = json.loads((root / meshes[name]['path']).read_text(encoding='utf-8'))
    return BVHTree.FromPolygons(m['vertices'], m['triangles'], all_triangles=True)


suppliers = {s['id']: s for s in manifest['suppliers']}
walls = {name: tree(name + '_Collision') for name in suppliers}
counts, grid, identities = Counter(), {}, set()
pitch = 2 * max(s['radius_m'] for s in suppliers.values())
minimum_clearance = float('inf')
with (root / 'test_charge.csv').open(encoding='utf-8', newline='') as f:
    for row in csv.DictReader(f):
        identity = int(row['id'])
        assert identity not in identities
        identities.add(identity)
        name = row['supplier']
        p = Vector([float(row[k + '_m']) for k in 'xyz'])
        radius = float(row['radius_m'])
        assert all(math.isfinite(v) for v in p)
        assert radius == suppliers[name]['radius_m']
        hit = walls[name].find_nearest(p)
        assert hit[0] is not None and hit[3] > radius, (name, identity, 'initial wall overlap')
        minimum_clearance = min(minimum_clearance, hit[3] - radius)
        key = tuple(math.floor(v / pitch) for v in p)
        for x in (-1, 0, 1):
            for y in (-1, 0, 1):
                for z in (-1, 0, 1):
                    for q, qr in grid.get((key[0] + x, key[1] + y, key[2] + z), []):
                        assert (p - q).length >= radius + qr - 1e-9, 'initial pair overlap'
        grid.setdefault(key, []).append((p, radius))
        counts[name] += 1
for name, supplier in suppliers.items():
    assert counts[name] == supplier['particle_count']

# The source already contains this reducer: differing port diameters are NOT
# themselves a disconnected-path error. Test through the actual source triangles.
reducer = tree('P202_Reducer_272_to_44')
clear_rays = 0
for i in range(64):
    angle = 2 * math.pi * i / 64
    start = Vector((3 + .018 * math.cos(angle), .018 * math.sin(angle), 1.30))
    assert reducer.ray_cast(start, Vector((0, 0, 1)), .42)[0] is None, 'reducer bore obstructed'
    clear_rays += 1
report = dict(initial_charge_valid=True, count=sum(counts.values()), suppliers=dict(counts),
              minimum_initial_wall_clearance_m=minimum_clearance,
              reducer_axial_clearance_rays=clear_rays,
              source_unchanged=True, simulation_executed=False,
              full_plant_lumen_validated=False, pressure_solver_integrated=False,
              end_to_end_transport_validated=False,
              limits=['Local seed and reducer checks only; not a whole-plant fluid seal or topology certificate.',
                      'A dedicated wall mesh is not a fluid-volume mesh.',
                      'Open mesh edges require interface classification; they are not automatically leaks.'])
(root / 'preflight.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
