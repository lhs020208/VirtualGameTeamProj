"""Apply a cheap, readable material and level-local postprocess to the current MX301 test.
No solver, collision mesh, particle initialization or time-step changes.
Run inside the editor: py "D:/GitHub/VirtualGameTeamProj/Scripts/simplify_powder_visuals.py"
"""
import unreal as u,json,hashlib
from pathlib import Path
root=Path('D:/GitHub/VirtualGameTeamProj')
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().startswith('/Game/DEM/Tests/L_MX301_BinaryMaterials.'), 'Open BinaryMaterials test first'
actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
solver=next(a for a in actors if a.get_class().get_name()=='PowderGranularMixer')
protected=['particle_count','time_step','contact_iterations','rpm','target_rate','binary_materials','enable_particle_rotation','rolling_friction']
before={k:solver.get_editor_property(k) for k in protected}
assets=['Content/DEM/Settings/DA_VirtualPowder_A.uasset','Content/DEM/Settings/DA_VirtualPowder_B.uasset','Content/DEM/SM_MX301_TestVessel.uasset','Content/DEM/SM_MX301_TestRotor.uasset']
hashes={p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in assets}
m=u.load_asset('/Game/M_BulkFlow');assert m
u.MaterialEditingLibrary.delete_all_material_expressions(m)
m.set_editor_property('shading_model',u.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('blend_mode',u.BlendMode.BLEND_OPAQUE)
m.set_editor_property('used_with_instanced_static_meshes',True)
m.set_editor_property('two_sided',False)
def node(cls,x,y):return u.MaterialEditingLibrary.create_material_expression(m,cls,x,y)
def link(a,b,pin):assert u.MaterialEditingLibrary.connect_material_expressions(a,'',b,pin)
tint=node(u.MaterialExpressionVectorParameter,-650,0);tint.set_editor_property('parameter_name','Tint');tint.set_editor_property('default_value',u.LinearColor(.4,.4,.4,1))
normal=node(u.MaterialExpressionPixelNormalWS,-900,200)
direction=node(u.MaterialExpressionConstant3Vector,-900,350);direction.set_editor_property('constant',u.LinearColor(.3,-.4,.8660254,1))
dot=node(u.MaterialExpressionDotProduct,-650,230);link(normal,dot,'A');link(direction,dot,'B')
sat=node(u.MaterialExpressionSaturate,-470,230);link(dot,sat,'')
scale=node(u.MaterialExpressionMultiply,-300,230);scale.set_editor_property('const_b',.55);link(sat,scale,'A')
ambient=node(u.MaterialExpressionAdd,-100,230);ambient.set_editor_property('const_b',.45);link(scale,ambient,'A')
color=node(u.MaterialExpressionMultiply,100,0);link(tint,color,'A');link(ambient,color,'B')
assert u.MaterialEditingLibrary.connect_material_property(color,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
u.MaterialEditingLibrary.recompile_material(m)
assert u.EditorAssetLibrary.save_loaded_asset(m)
pp=next((a for a in actors if a.get_actor_label()=='Powder_SimpleDisplay'),None)
if not pp:pp=u.get_editor_subsystem(u.EditorActorSubsystem).spawn_actor_from_class(u.PostProcessVolume,u.Vector())
pp.set_actor_label('Powder_SimpleDisplay');pp.set_editor_property('unbound',True);pp.set_editor_property('priority',100.)
s=pp.get_editor_property('settings')
values={'dynamic_global_illumination_method':u.DynamicGlobalIlluminationMethod.NONE,'reflection_method':u.ReflectionMethod.NONE,'ambient_occlusion_intensity':0.,'bloom_intensity':0.,'motion_blur_amount':0.,'auto_exposure_method':u.AutoExposureMethod.AEM_MANUAL,'auto_exposure_apply_physical_camera_exposure':False,'auto_exposure_bias':0.,'local_exposure_highlight_contrast_scale':1.,'local_exposure_shadow_contrast_scale':1.}
for k,v in values.items():s.set_editor_property('override_'+k,True);s.set_editor_property(k,v)
pp.set_editor_property('settings',s)
# These actors only provide display lighting. Physics is the independent GPU solver.
for a in actors:
 for c in a.get_components_by_class(u.StaticMeshComponent):c.set_cast_shadow(False)
 for c in a.get_components_by_class(u.LightComponent):c.set_editor_property('cast_shadows',False)
after={k:solver.get_editor_property(k) for k in protected};assert before==after
assert hashes=={p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in assets}
assert u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
report={'physics_unchanged':before==after,'physics':after,'protected_asset_sha256':hashes,'material':'/Game/M_BulkFlow','shading':'unlit opaque, Tint * (0.45 + 0.55 * saturate(dot(normal, fixed direction)))','effects':{k:str(v) for k,v in values.items()},'scope':'shared simulation material and BinaryMaterials postprocess; geometry and solver untouched'}
(root/'Saved/simple_visuals_result.json').write_text(json.dumps(report,indent=2))
u.log('POWDER_SIMPLE_VISUALS_SAVED')

