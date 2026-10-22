import bpy
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2,radius=.5)
o=bpy.context.object;o.name='SM_GrainSphere'
for p in o.data.polygons:p.use_smooth=True
bpy.ops.export_scene.fbx(filepath='D:/GitHub/VirtualGameTeamProj/SourceAssets/Granular/SM_GrainSphere.fbx',use_selection=True,object_types={'MESH'},apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',bake_anim=False)

