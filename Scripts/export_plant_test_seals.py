"""Background Blender only: render meshes for explicit test sealing overrides."""
import bpy,json,math
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parents[1]/'SourceAssets/ProcessPlantPhysics'
model=json.loads((root/'runtime_manifest.json').read_text(encoding='utf-8'))
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
for spec in model['test_overrides']:
 if spec['kind']!='open_sleeve':continue
 a=Vector(spec['a']);b=Vector(spec['b']);axis=(b-a).normalized();u=axis.cross(Vector((0,0,1)) if abs(axis.z)<.9 else Vector((0,1,0))).normalized();v=axis.cross(u)
 verts=[];faces=[]
 for point,radius in [(a,spec['radius_a_m']),(b,spec['radius_b_m'])]:
  for j in range(64):verts.append(point+radius*(u*math.cos(j*math.tau/64)+v*math.sin(j*math.tau/64)))
 for j in range(64):k=(j+1)%64;faces.append((j,k,k+64,j+64))
 mesh=bpy.data.meshes.new(spec['id']);mesh.from_pydata(verts,[],faces);obj=bpy.data.objects.new(spec['id'],mesh);bpy.context.collection.objects.link(obj)
bpy.ops.export_scene.fbx(filepath=str(root/'TestSeals.fbx'),object_types={'MESH'},use_mesh_modifiers=True,add_leaf_bones=False,axis_forward='-Y',axis_up='Z',bake_anim=False)
