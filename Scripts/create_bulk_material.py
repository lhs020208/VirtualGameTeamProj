import unreal as u
path='/Game/M_BulkFlow'
if not u.EditorAssetLibrary.does_asset_exist(path):
 m=u.AssetToolsHelpers.get_asset_tools().create_asset('M_BulkFlow','/Game',u.Material,u.MaterialFactoryNew())
 tint=u.MaterialEditingLibrary.create_material_expression(m,u.MaterialExpressionVectorParameter,-200,0)
 tint.set_editor_property('parameter_name','Tint');tint.set_editor_property('default_value',u.LinearColor(.2,.2,.2,1))
 u.MaterialEditingLibrary.connect_material_property(tint,'',u.MaterialProperty.MP_BASE_COLOR)
 u.MaterialEditingLibrary.recompile_material(m);assert u.EditorAssetLibrary.save_loaded_asset(m)
m=u.load_asset(path)
m.set_editor_property('used_with_instanced_static_meshes',True)
u.MaterialEditingLibrary.recompile_material(m)
assert u.EditorAssetLibrary.save_loaded_asset(m)
u.log('BULK_MATERIAL_READY')
