import bpy,json,bmesh
from mathutils import Vector
from pathlib import Path
report={'source':bpy.data.filepath,'unit_scale':bpy.context.scene.unit_settings.scale_length,'objects':[]}
for o in bpy.context.scene.objects:
 if o.type=='MESH' and any(s in o.name for s in ['Interior','Collision','Shaft','Rotor','Render_Shell','Cube']):
  ev=o.evaluated_get(bpy.context.evaluated_depsgraph_get());mesh=ev.to_mesh();bm=bmesh.new();bm.from_mesh(mesh);bm.transform(o.matrix_world)
  pts=[o.matrix_world@Vector(p) for p in ev.bound_box]
  report['objects'].append({'name':o.name,'bounds_m':[[min(p[k] for p in pts),max(p[k] for p in pts)] for k in range(3)],'closed':all(e.is_manifold for e in bm.edges),'volume_m3':abs(bm.calc_volume()),'vertices':len(mesh.vertices),'location':list(o.matrix_world.translation)})
  bm.free();ev.to_mesh_clear()
Path('D:/GitHub/VirtualGameTeamProj/Docs/DEM/optimization37_44/geometry_readback.json').write_text(json.dumps(report,indent=2),encoding='utf-8')

