"""Read-only source audit and FBX export in an isolated Blender process."""
import bpy, json, hashlib, sys
from pathlib import Path
from mathutils import Vector
root=Path('D:/GitHub/VirtualGameTeamProj')
out=root/'SourceAssets/ProcessPlant_20261110_LowPoly';out.mkdir(parents=True,exist_ok=True)
docs=root/'Docs/ProcessPlant_20261110_LowPoly'
source=Path(bpy.data.filepath)
assert hashlib.sha256(source.read_bytes()).hexdigest()=='93d21f408c7c4d0f2e22252acfb9b285bf6ba5a72922d63e8c4ebdfd52e84034'
assert bpy.context.scene.unit_settings.scale_length==1
# Record visibility before enabling collider evaluation.
visible=[o for o in bpy.context.visible_objects if o.type in {'MESH','CURVE','FONT','EMPTY'} and not o.hide_render and not any(w in o.name.lower() for w in ['collision','simulation','preview_ground','studio_floor'])]
visible=[o for o in visible if 'ClosedSeal' not in o.name and not (o.type=='MESH' and len(o.data.polygons)==0)]
def bounds(o):
 if o.type=='MESH':
  evaluated=o.evaluated_get(bpy.context.evaluated_depsgraph_get());mesh=evaluated.to_mesh()
  q=[evaluated.matrix_world@v.co for v in mesh.vertices];evaluated.to_mesh_clear()
 else:q=[o.matrix_world@Vector(v) for v in o.bound_box]
 return [[min(v[k] for v in q) for k in range(3)],[max(v[k] for v in q) for k in range(3)]]
bpy.context.view_layer.update()
records=[dict(name=o.name,type=o.type,bounds=bounds(o),matrix=[list(r) for r in o.matrix_world],vertices=len(o.data.vertices) if o.type=='MESH' else 0,properties={k:str(o[k]) for k in o.keys()}) for o in visible]
(docs/'visual_source.json').write_text(json.dumps(dict(source=str(source),sha256=hashlib.sha256(source.read_bytes()).hexdigest(),units='metres',objects=records),ensure_ascii=False,indent=2),encoding='utf8')
(out/'suppliers.json').write_text(json.dumps({o.name:bounds(o) for o in visible if 'UpperBody_Render' in o.name},indent=2))
bpy.ops.object.select_all(action='DESELECT')
for o in visible:o.select_set(True)
bpy.ops.export_scene.fbx(filepath=str(out/'ProcessPlant_LowPoly.fbx'),use_selection=True,add_leaf_bones=False,bake_anim=False,axis_forward='-Y',axis_up='Z',path_mode='AUTO')
ns={'__name__':'physics_export'}
exec(compile((root/'Scripts/export_plant_physics_source.py').read_text(),'export_physics','exec'),ns)
ns['export'](root/'Intermediate/PlantLowPoly20261110/Physics')
print('LOWPOLY_EXPORT_COMPLETE',len(visible))
