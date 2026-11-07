import bpy,json
from pathlib import Path
from mathutils import Vector
p=Path('D:/GitHub/VirtualGameTeamProj/SourceAssets/ProcessPlant_20261104')
scene=bpy.context.scene
def bounds(o):
 pts=[o.matrix_world@Vector(v) for v in o.bound_box]
 return [[fn(v[k] for v in pts) for k in range(3)] for fn in (min,max)]
body={o.name:bounds(o) for o in scene.objects if 'UpperBody_Render' in o.name}
objs=[o for o in bpy.context.visible_objects if o.type in {'MESH','CURVE','FONT','EMPTY'} and not o.hide_render and not any(w in o.name.lower() for w in ['collision','simulation','preview_ground','studio_floor'])]
bpy.ops.object.select_all(action='DESELECT')
for o in objs:o.select_set(True)
(p/'suppliers.json').write_text(json.dumps(body,indent=2))
print('EXPORT',len(objs),body,flush=True)
bpy.ops.export_scene.fbx(filepath=str(p/'ProcessPlant_20261104.fbx'),use_selection=True,add_leaf_bones=False,bake_anim=False,axis_forward='-Y',axis_up='Z',path_mode='AUTO')
