import bpy,bmesh,json,math
from pathlib import Path
from mathutils import Vector,Matrix
out=Path('D:/GitHub/VirtualGameTeamProj/SourceAssets/Granular');out.mkdir(exist_ok=True,parents=True)
def components(obj,shift):
 mesh=obj.evaluated_get(bpy.context.evaluated_depsgraph_get()).to_mesh()
 vertices=[obj.matrix_world@v.co-Vector(shift) for v in mesh.vertices]
 adjacency=[set() for v in vertices]
 for edge in mesh.edges:
  a,b=edge.vertices;adjacency[a].add(b);adjacency[b].add(a)
 pending=set(range(len(vertices)));result=[]
 while pending:
  stack=[pending.pop()];seen=set(stack)
  while stack:
   for j in adjacency[stack.pop()]:
    if j in pending:pending.remove(j);seen.add(j);stack.append(j)
  planes=[]
  for poly in mesh.polygons:
   if poly.vertices[0] not in seen:continue
   pts=[vertices[j] for j in poly.vertices];normal=(pts[1]-pts[0]).cross(pts[2]-pts[0])
   if normal.length<1e-10:continue
   normal.normalize();distance=normal.dot(pts[0]);plane=[*normal,distance]
   if not any(max(abs(a-b) for a,b in zip(plane,p))<1e-5 for p in planes):planes.append(plane)
  violation=max((Vector(p[:3]).dot(vertices[j])-p[3] for p in planes for j in seen),default=0)
  result.append(dict(vertices=len(seen),planes=planes,convex_violation=violation,bounds=[[min(vertices[j][k] for j in seen),max(vertices[j][k] for j in seen)] for k in range(3)]))
 return result
interior=components(bpy.data.objects['MX-301_Interior'],[0,0,0]);rotor=components(bpy.data.objects['MX-301_Collision_Rotor'],[3,0,.9])
data=dict(source=bpy.data.filepath,interior=interior,rotor=rotor,test_only=True)
(out/'MX301_convex.json').write_text(json.dumps(data,indent=2))
print('CONVEX_SUMMARY',[(p['vertices'],len(p['planes']),p['convex_violation']) for p in interior+rotor])
for name,source,shift,cutaway in [('SM_MX301_TestVessel','MX-301_Collision_Vessel',[3,0,.9],True),('SM_MX301_TestRotor','MX-301_Collision_Rotor',[3,0,.9],False)]:
 original=bpy.data.objects[source];obj=bpy.data.objects.new(name,bpy.data.meshes.new_from_object(original.evaluated_get(bpy.context.evaluated_depsgraph_get())));bpy.context.collection.objects.link(obj);obj.matrix_world=original.matrix_world.copy();obj.hide_set(False);obj.hide_render=False;obj.hide_viewport=False
 matrix=obj.matrix_world.copy();obj.matrix_world=Matrix.Identity(4)
 for v in obj.data.vertices:v.co=matrix@v.co-Vector(shift)
 if cutaway:
  bm=bmesh.new();bm.from_mesh(obj.data);bmesh.ops.delete(bm,geom=[f for f in bm.faces if f.calc_center_median().y<-.04],context='FACES');bm.to_mesh(obj.data);bm.free()
 bm=bmesh.new();bm.from_mesh(obj.data);bm.transform(Matrix.Diagonal((1,-1,1,1)));bmesh.ops.reverse_faces(bm,faces=list(bm.faces));bm.to_mesh(obj.data);bm.free()
 bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
 bpy.ops.export_scene.fbx(filepath=str(out/(name+'.fbx')),use_selection=True,object_types={'MESH'},apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',bake_space_transform=False,bake_anim=False,axis_forward='-Y',axis_up='Z',add_leaf_bones=False)
 bpy.data.objects.remove(obj,do_unlink=True)
