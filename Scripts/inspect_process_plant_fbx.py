"""Read the delivered FBX in an isolated Blender process; no export or blend save."""
import bpy, json, hashlib
from pathlib import Path
from mathutils import Vector
source=Path('D:/GitHub/가상현실게임/Exports/ProcessPlant_20261029/ProcessPlant_Assembly.fbx')
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(source))
rows=[]
for obj in bpy.context.scene.objects:
    corners=[list(obj.matrix_world @ Vector(c)) for c in obj.bound_box] if obj.type=='MESH' else []
    rows.append(dict(name=obj.name,type=obj.type,parent=obj.parent.name if obj.parent else None,
        world_matrix=[list(row) for row in obj.matrix_world],bounds=corners,
        vertices=len(obj.data.vertices) if obj.type=='MESH' else 0,
        materials=[s.material.name if s.material else None for s in obj.material_slots] if obj.type=='MESH' else [],
        used_material_indices=sorted(set(p.material_index for p in obj.data.polygons)) if obj.type=='MESH' else [],
        world_vertices=[list(obj.matrix_world@v.co) for v in obj.data.vertices] if obj.type=='MESH' and 'ClampLever' in obj.name else []))
out=Path('D:/GitHub/VirtualGameTeamProj/Docs/ProcessPlant_20261029')
out.mkdir(parents=True,exist_ok=True)
(out/'source_fbx_inventory.json').write_text(json.dumps(dict(sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
    source=str(source),scene_unit_scale=bpy.context.scene.unit_settings.scale_length,objects=rows,
    images=[dict(name=i.name,path=i.filepath,packed=bool(i.packed_file)) for i in bpy.data.images],
    materials=[dict(name=m.name,diffuse=list(m.diffuse_color)) for m in bpy.data.materials]),ensure_ascii=False,indent=2),encoding='utf-8')
print('FBX_STATIC_INSPECTION_COMPLETE',len(rows))
