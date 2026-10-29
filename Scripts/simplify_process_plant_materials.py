"""Simplify only the imported process plant. Editor-only; no simulation."""
import json, re
from pathlib import Path
import unreal as u
ROOT='/Game/ProcessPlant_20261029'
OUT=Path('D:/GitHub/VirtualGameTeamProj/Docs/ProcessPlant_20261029')
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().startswith(ROOT+'/L_ProcessPlant_Assembly.'), 'Open the new plant level first'
source=json.loads((OUT/'source_fbx_inventory.json').read_text(encoding='utf-8'))
def key(name):return re.sub(r'[^a-zA-Z0-9_-]','_',name)
colors={key(m['name']):m['diffuse'] for m in source['materials']}
path=ROOT+'/Materials/M_PlantSimple'
m=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else u.AssetToolsHelpers.get_asset_tools().create_asset('M_PlantSimple',ROOT+'/Materials',u.Material,u.MaterialFactoryNew())
u.MaterialEditingLibrary.delete_all_material_expressions(m)
m.set_editor_property('shading_model',u.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('blend_mode',u.BlendMode.BLEND_OPAQUE)
m.set_editor_property('two_sided',False)
def node(cls,x,y):return u.MaterialEditingLibrary.create_material_expression(m,cls,x,y)
def link(a,b,pin):assert u.MaterialEditingLibrary.connect_material_expressions(a,'',b,pin)
tint=node(u.MaterialExpressionVectorParameter,-650,0)
tint.set_editor_property('parameter_name','Tint')
tint.set_editor_property('default_value',u.LinearColor(.4,.4,.4,1))
normal=node(u.MaterialExpressionPixelNormalWS,-900,200)
direction=node(u.MaterialExpressionConstant3Vector,-900,350)
direction.set_editor_property('constant',u.LinearColor(.3,-.4,.8660254,1))
dot=node(u.MaterialExpressionDotProduct,-650,230);link(normal,dot,'A');link(direction,dot,'B')
sat=node(u.MaterialExpressionSaturate,-470,230);link(dot,sat,'')
scale=node(u.MaterialExpressionMultiply,-300,230);scale.set_editor_property('const_b',.55);link(sat,scale,'A')
ambient=node(u.MaterialExpressionAdd,-100,230);ambient.set_editor_property('const_b',.45);link(scale,ambient,'A')
color=node(u.MaterialExpressionMultiply,100,0);link(tint,color,'A');link(ambient,color,'B')
assert u.MaterialEditingLibrary.connect_material_property(color,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
u.MaterialEditingLibrary.recompile_material(m)
assert u.EditorAssetLibrary.save_loaded_asset(m)
changes=[]
for path in u.EditorAssetLibrary.list_assets(ROOT+'/Assets/Materials',recursive=True,include_folder=False):
    instance=u.load_asset(path)
    assert isinstance(instance,u.MaterialInstanceConstant)
    assert instance.get_name() in colors, instance.get_name()
    rgba=colors[instance.get_name()]
    old=instance.get_editor_property('parent').get_path_name()
    u.MaterialEditingLibrary.set_material_instance_parent(instance,m)
    u.MaterialEditingLibrary.update_material_instance(instance)
    # UE 5.8's setter returns false even after a successful write; read back instead.
    u.MaterialEditingLibrary.set_material_instance_vector_parameter_value(instance,'Tint',u.LinearColor(*rgba))
    actual=u.MaterialEditingLibrary.get_material_instance_vector_parameter_value(instance,'Tint')
    assert max(abs(x-y) for x,y in zip((actual.r,actual.g,actual.b,actual.a),rgba))<1e-6
    u.MaterialEditingLibrary.update_material_instance(instance)
    assert u.EditorAssetLibrary.save_loaded_asset(instance)
    changes.append(dict(material=path,original_parent=old,tint=rgba,new_parent=m.get_path_name()))
assert len(changes)==30
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
pp=actors.spawn_actor_from_class(u.PostProcessVolume,u.Vector())
pp.set_actor_label('Plant_SimpleDisplay')
pp.set_editor_property('unbound',True);pp.set_editor_property('priority',100.)
s=pp.get_editor_property('settings')
settings={'dynamic_global_illumination_method':u.DynamicGlobalIlluminationMethod.NONE,
    'reflection_method':u.ReflectionMethod.NONE,'ambient_occlusion_intensity':0.,
    'bloom_intensity':0.,'motion_blur_amount':0.,'auto_exposure_method':u.AutoExposureMethod.AEM_MANUAL,
    'auto_exposure_apply_physical_camera_exposure':False,'auto_exposure_bias':0.,
    'local_exposure_highlight_contrast_scale':1.,'local_exposure_shadow_contrast_scale':1.}
for k,v in settings.items():s.set_editor_property('override_'+k,True);s.set_editor_property(k,v)
pp.set_editor_property('settings',s)
camera=u.Vector(570,470,430);target=u.Vector(150,-50,200)
u.get_editor_subsystem(u.UnrealEditorSubsystem).set_level_viewport_camera_info(camera,u.MathLibrary.find_look_at_rotation(camera,target))
assert u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
(OUT/'simple_materials.json').write_text(json.dumps(dict(master=m.get_path_name(),instances=changes,
    shading='Unlit opaque; Tint*(0.45+0.55*saturate(dot(PixelNormalWS,fixed direction)))',
    texture_samples=0,simulation_executed=False,postprocess={k:str(v) for k,v in settings.items()}),ensure_ascii=False,indent=2),encoding='utf-8')
u.log('PROCESS_PLANT_SIMPLE_MATERIALS_SAVED')
