"""Compare static FBX/Unreal manifests (no engine execution)."""
import json
from pathlib import Path
import numpy as np
from scipy.spatial import cKDTree
out=Path(__file__).resolve().parents[1]/'Docs/ProcessPlant_20261029'
source=json.loads((out/'source_fbx_inventory.json').read_text(encoding='utf-8'))
reload=json.loads((out/'reload_validation.json').read_text(encoding='utf-8'))
imported=json.loads((out/'import_report.json').read_text(encoding='utf-8'))
actors={a['label']:a for a in reload['actors']}
meshes={a['label']:a for a in imported['actors']}
def normalize(name):return name.replace('.','_').replace(' ','_')
handedness=np.diag([1,-1,1])
missing=[];parents=[];pivot=[];basis=[];bounds=[];material_slots=[];unused_slots=[];bounds_exceptions=[]
lever_data={a['name']:a for a in json.loads((out/'lever_geometry.json').read_text(encoding='utf-8'))}
for obj in source['objects']:
    name=normalize(obj['name'])
    if name not in actors:missing.append(name);continue
    a=actors[name]; matrix=np.array(obj['world_matrix'])
    pivot.append(float(np.max(np.abs(handedness@matrix[:3,3]*100-np.array(a['origin'])))))
    actual_basis=(np.array(a['basis']).T-np.array(a['origin'])[:,None])/100
    basis.append(float(np.max(np.abs(actual_basis-handedness@matrix[:3,:3]@handedness))))
    expected_parent=normalize(obj['parent']) if obj['parent'] else 'RootNode'
    if a['parent']!=expected_parent:parents.append(name)
    if obj['type']=='MESH':
        mesh=meshes[name]; points=np.array(obj['bounds'])@handedness*100
        lo=np.array(mesh['bounds_origin'])-np.array(mesh['bounds_extent'])
        hi=np.array(mesh['bounds_origin'])+np.array(mesh['bounds_extent'])
        error=float(max(np.max(np.abs(points.min(axis=0)-lo)),np.max(np.abs(points.max(axis=0)-hi))))
        if error>.01 and name in lever_data:
            p=np.array(obj['world_vertices'])@handedness*100;v=np.array(lever_data[name]['vertices'])
            vertex_error=float(max(cKDTree(v).query(p)[0].max(),cKDTree(p).query(v)[0].max()))
            bounds_exceptions.append(dict(name=name,box_error_cm=error,vertex_error_cm=vertex_error,
                source_vertex_count=len(p),imported_vertex_count=len(v)))
            error=vertex_error if len(p)==len(v) else error
        bounds.append(error)
        if len(obj['materials'])!=len(mesh['materials']):
            if obj['used_material_indices']==[0] and len(mesh['materials'])==1:unused_slots.append(name)
            else:material_slots.append(name)
report=dict(source_nodes=len(source['objects']),source_meshes=len(meshes),missing=missing,
    parent_mismatches=parents,max_pivot_error_cm=max(pivot),max_basis_error=max(basis),
    max_bounds_error_cm=max(bounds),material_slot_count_mismatches=material_slots,
    unused_material_slots_compacted=unused_slots,rotated_bounds_verified_by_world_vertices=bounds_exceptions,
    source_images=source['images'],asset_classes=reload['asset_classes'],
    note='FBX meters converted to UE centimeters with Y handedness reflection; scene root scale 100 retained.')
report['passed']=not(missing or parents or material_slots) and max(pivot)<.001 and max(basis)<.001 and max(bounds)<.01
(out/'source_comparison.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))
assert report['passed']
