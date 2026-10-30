"""Read-only Blender source export. Run in an isolated Blender background process.

Exports source-space SI geometry and ports, never rendered cutaway geometry.
Source/target and supplier count are not embedded in the solver.
"""
import argparse
import hashlib
import json
import re
import sys
from collections import Counter
from pathlib import Path

import bpy
import bmesh
from mathutils import Vector


def plain(value):
    if hasattr(value, 'to_list'):
        return [plain(v) for v in value.to_list()]
    if hasattr(value, 'to_dict'):
        return {k: plain(v) for k, v in value.to_dict().items()}
    if isinstance(value, (str, bool, int, float)) or value is None:
        return value
    return str(value)


def export(output):
    source = Path(bpy.data.filepath)
    scale = bpy.context.scene.unit_settings.scale_length
    if abs(scale - 1) > 1e-9:
        raise ValueError('Expected confirmed metre source; explicit unit conversion required')
    output.mkdir(parents=True, exist_ok=True)
    ports, meshes, suppliers = [], [], []
    # Excluded collections can leave matrix_world unevaluated (identity). Enable
    # evaluation ONLY in this disposable background process; never save the blend.
    def include(layer):
        layer.exclude = False
        layer.hide_viewport = False
        layer.collection.hide_viewport = False
        for child in layer.children:
            include(child)
    include(bpy.context.view_layer.layer_collection)
    for obj in bpy.data.objects:
        obj.hide_viewport = False
        obj.hide_set(False)
    bpy.context.view_layer.update()
    graph = bpy.context.evaluated_depsgraph_get()
    for obj in sorted(bpy.data.objects, key=lambda o: o.name):
        props = {k: plain(obj[k]) for k in obj.keys() if k != '_RNA_UI'}
        if obj.type == 'EMPTY' and 'PORT' in obj.name.upper():
            ports.append(dict(id=obj.name, position_m=list(obj.matrix_world.translation),
                              properties=props))
        dedicated = obj.type == 'MESH' and 'collision' in obj.name.lower()
        # Also retain physical pipe walls: these were omitted from dedicated collider collections.
        pipe = obj.type == 'MESH' and ('flow_path_open' in props or obj.name == 'P202_Reducer_272_to_44' or
            obj.name.startswith(('PL_120_', 'PL_130_', 'PL_140_', 'FH122S_110_',
                                 'DP_110_', 'DP_120_', 'DP_130_', 'RH201_110_',
                                 'RH201_120_', 'RH201_121_', 'RH201_130_', 'RH201_140_',
                                 'RH201_210_', 'RH201_211_', 'RH201_215_',
                                 'BV122_PIPE_', 'BV272_PIPE_', 'BV122_110_', 'BV122_111_', 'BV272_110_', 'BV272_111_',
                                 'PL_C', 'RH201_C', 'RH201_Filter_BottomCap')))
        if not (dedicated or pipe):
            continue
        evaluated = obj.evaluated_get(graph)
        mesh = evaluated.to_mesh()
        try:
            placement = evaluated.matrix_world.copy()
            placement_reference = None
            bm = bmesh.new()
            bm.from_mesh(mesh)
            bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=1e-7)
            bmesh.ops.triangulate(bm, faces=list(bm.faces))
            bm.verts.ensure_lookup_table()
            bm.verts.index_update()
            vertices = [list(placement @ v.co) for v in bm.verts]
            triangles = [[v.index for v in f.verts] for f in bm.faces]
            bm.free()
            edges = Counter(tuple(sorted((t[i], t[(i + 1) % 3]))) for t in triangles for i in range(3))
            lo = [min(v[k] for v in vertices) for k in range(3)]
            hi = [max(v[k] for v in vertices) for k in range(3)]
            data = dict(id=obj.name, units='m', coordinates='Blender world XYZ, Z up',
                        vertices=vertices, triangles=triangles, properties=props,
                        source_matrix=[list(r) for r in evaluated.matrix_world],
                        placement_matrix=[list(r) for r in placement],
                        placement_reference=placement_reference,
                        weld_tolerance_m=1e-7)
            path = obj.name.replace('/', '_') + '.json'
            (output / path).write_text(json.dumps(data, separators=(',', ':')), encoding='utf-8')
            meshes.append(dict(id=obj.name, path=path, bounds_m=[lo, hi],
                               vertices=len(vertices), triangles=len(triangles),
                               open_edges=sum(n == 1 for n in edges.values()),
                               nonmanifold_edges=sum(n > 2 for n in edges.values()),
                               placement_reference=placement_reference,
                               dedicated_collision=dedicated, properties=props))
        finally:
            evaluated.to_mesh_clear()
    for obj in sorted(bpy.data.objects, key=lambda o: o.name):
        if re.fullmatch(r'H-\d+[A-Za-z0-9]*_UpperBody_Render', obj.name):
            corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
            lo = [min(v[k] for v in corners) for k in range(3)]
            hi = [max(v[k] for v in corners) for k in range(3)]
            suppliers.append(dict(id=obj.name.removesuffix('_UpperBody_Render'),
                                  cylinder_bounds_m=[lo, hi], particle_count=10000,
                                  radius_m=0.00125, density_kg_m3=1200,
                                  material='virtual_test_material', calibrated=False))
    result = dict(schema='plant-physics-source/1', source=str(source),
                  sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                  coordinates='Blender world metres; UE = 100 * (x,-y,z)',
                  suppliers=suppliers, ports=ports, meshes=meshes,
                  pressure_boundary_kind='adjustable_test_absolute_pressure',
                  ready_for_simulation=False,
                  note='Raw source evidence. Closed wall solids are not closed fluid volumes; assembly and lumen audit required.')
    (output / 'manifest.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print('PLANT_PHYSICS_EXPORTED', len(meshes), 'meshes', len(ports), 'ports', len(suppliers), 'suppliers')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', required=True)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    export(Path(args.output))
