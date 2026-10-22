import unreal as u
t=u.AssetImportTask();t.filename='D:/GitHub/VirtualGameTeamProj/SourceAssets/Granular/SM_GrainSphere.fbx';t.destination_path='/Game/DEM';t.destination_name='SM_GrainSphere';t.automated=True;t.save=True;t.replace_existing=True
o=u.FbxImportUI();o.import_mesh=True;o.import_materials=False;o.import_textures=False;o.import_as_skeletal=False;o.mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH;o.static_mesh_import_data.combine_meshes=True;o.static_mesh_import_data.auto_generate_collision=False;t.options=o
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t]);mesh=u.load_asset('/Game/DEM/SM_GrainSphere');assert mesh
u.log('GRAIN_SPHERE_BOUNDS '+str(mesh.get_bounds()))

