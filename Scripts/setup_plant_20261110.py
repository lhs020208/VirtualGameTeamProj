"""Install interactive controls into the November plant using its own runtime data."""
import json,re
from pathlib import Path
import unreal as u
project=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir()))
root='/Game/ProcessPlant_20261110_LowPoly'
level=root+'/L_ProcessPlant_LowPoly'
levels=u.get_editor_subsystem(u.LevelEditorSubsystem); actors=u.get_editor_subsystem(u.EditorActorSubsystem)
assert levels.load_level(level)
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
for a in actors.get_all_level_actors():
 if 'ClosedSeal' in a.get_actor_label():actors.destroy_actor(a)
inventory={a.get_actor_label():a for a in actors.get_all_level_actors() if isinstance(a,u.StaticMeshActor)}
assert len(inventory)>2000
pclass=u.load_class(None,'/Script/VirtualGameTeamProj.PowderPlantTest')
dclass=u.load_class(None,'/Script/VirtualGameTeamProj.PlantVisibilityController')
def singleton(cls):
 matches=[a for a in actors.get_all_level_actors() if a.get_class()==cls]
 for extra in matches[1:]:actors.destroy_actor(extra)
 return matches[0] if matches else actors.spawn_actor_from_class(cls,u.Vector())
test=singleton(pclass);test.set_actor_label('Plant_Pressure_Grains_LowPoly')
test.set_editor_property('data_directory','SourceAssets/ProcessPlant_20261110_LowPoly/Simulation')
test.set_editor_property('mixer_pivot_m',u.Vector(3,0,1.105))
test.set_editor_property('mixer_bounds_min_m',u.Vector(2.6225,-.2265,.941))
test.set_editor_property('mixer_bounds_max_m',u.Vector(3.3775,.2265,1.31))
test.set_editor_property('regulated_pressure_enabled',True)
assert test.apply_pressure_preset(0)
test.reset_simulation()
assert not str(test.get_editor_property('error')),str(test.get_editor_property('error'))
assert [c.get_instance_count() for c in test.get_components_by_class(u.InstancedStaticMeshComponent) if c.get_instance_count()]==[10000,10000]
controller=singleton(dclass);controller.set_actor_label('Plant_DisplayControls')
parts=[];frames=0;cuts=0
cutroot='/Game/ProcessPlant_20261029/Materials'
fallback=u.load_asset(cutroot+'/M_PlantCutaway');assert fallback
materials={}
for name,actor in inventory.items():
 comp=actor.static_mesh_component
 frame=bool(re.match(r'(MX301_Pedestal_|P10[12]_Rack|P201_Stand_|P201_(Horizontal|Riser)SupportClamp|RH_Frame_|HLCM_|WH101_(Frame_|Foot_Anchor|Lower_Frame_Brace)|MX-301_(Base_Frame|Frame_|Support_|Rod_Foot|Vessel_Saddle)|H-101[A-F]_(Brace_|Bracket_|Foot_|Leg_|PneumaticFrame|PneumaticMount)|AIR101_(RiserPost|RiserClip|RiserBase|ClipFoot))',name))
 ref=None
 if not frame:
  if re.match(r'H-101[A-F]_(UpperBody|LowerCone|ClampGasket|ClampRing|Rim_|Label|Nameplate|Lid)',name):ref=name[:6]+'_UpperBody_Render'
  elif re.match(r'WH101_(Body|Cone|Inlet|Top|Separate|Inspection|Outlet)',name):ref='WH101_Body_ID700'
  elif name.startswith('RH201_') and not name.startswith('RH201_Filter_'):ref='RH201_110_Main_shell'
  elif re.match(r'MX-301_(Render_Shell|Left_EndPlate|Right_EndPlate|Lid_|Handle_|Inlet_)',name):ref='MX-301_Render_Shell'
  elif re.match(r'(P10[12][A-F]_|P201_(Expansion|LowRoute|VT_)|P202_|PL_|FH122S_|CL122_|V101[A-F]_(Body|Union_)|BV(122|272)_(PIPE_|11[0-6]_)|ASSUMED_(Transition_|VT_(BoreBody|EndFlange|Housing)))',name):ref=name
 part=u.PlantVisualPart();part.actor=actor;part.frame=frame;part.cutaway=bool(ref);frames+=frame
 if ref:
  assert ref in inventory,ref
  origin,_=inventory[ref].get_actor_bounds(False);transform=comp.get_world_transform()
  part.local_plane_origin=transform.inverse_transform_location(origin)
  part.local_plane_normal=transform.rotation.unrotate_vector(u.Vector(0,1,0))*transform.scale3d
  cuts+=1
 for i in range(comp.get_num_materials()):
  original=comp.get_material(i);label=original.get_name().lower() if original else ''
  kind='blue' if 'blue' in label else 'black' if any(t in label for t in ['black','dark']) else 'bronze' if any(t in label for t in ['bronze','brass']) else 'white' if 'white' in label else 'grey'
  key=(kind,bool(ref))
  if key not in materials:
   path=root+'/Materials/MI_'+kind+('_Cut' if ref else '_Solid')
   mat=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else u.AssetToolsHelpers.get_asset_tools().create_asset(path.split('/')[-1],root+'/Materials',u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
   parent=fallback if ref else u.load_asset(cutroot+'/M_PlantSimple')
   u.MaterialEditingLibrary.set_material_instance_parent(mat,parent)
   tint={'blue':(.01,.25,.5,1),'black':(.025,.04,.05,1),'bronze':(.38,.25,.10,1),'white':(.65,.68,.7,1),'grey':(.4,.45,.48,1)}[kind]
   u.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mat,'Tint',u.LinearColor(*tint));u.MaterialEditingLibrary.update_material_instance(mat)
   assert u.EditorAssetLibrary.save_loaded_asset(mat);materials[key]=mat
  comp.set_material(i,materials[key])
 comp.set_collision_enabled(u.CollisionEnabled.NO_COLLISION);comp.set_cast_shadow(False)
 parts.append(part)
cam=singleton(u.load_class(None,'/Script/Engine.CameraActor'));cam.set_actor_label('Plant_OverviewCamera')
cam.set_actor_location(u.Vector(570,550,430),False,False);cam.set_actor_rotation(u.MathLibrary.find_look_at_rotation(cam.get_actor_location(),u.Vector(100,0,195)),False)
controller.parts=parts;controller.overview_camera=cam;controller.show_cutaway();controller.set_frames_visible(False)
world.get_world_settings().set_editor_property('default_game_mode',u.load_class(None,'/Script/VirtualGameTeamProj.PowderBulkGameMode'))
for a in actors.get_all_level_actors():
 if a.get_actor_label() in ['Initial_Powder_A','Initial_Powder_B']:actors.destroy_actor(a)
light=singleton(u.load_class(None,'/Script/Engine.DirectionalLight'));light.set_actor_label('Preview_Sun');light.set_actor_rotation(u.Rotator(-40,-35,0),False);light.light_component.set_editor_property('cast_shadows',False)
singleton(u.load_class(None,'/Script/Engine.SkyLight'));singleton(u.load_class(None,'/Script/Engine.SkyAtmosphere'))
pp=singleton(u.load_class(None,'/Script/Engine.PostProcessVolume'));pp.set_actor_label('Plant_SimpleDisplay');pp.set_editor_property('unbound',True);pp.set_editor_property('priority',100.)
settings=pp.get_editor_property('settings')
for k,v in {'dynamic_global_illumination_method':u.DynamicGlobalIlluminationMethod.NONE,'reflection_method':u.ReflectionMethod.NONE,'ambient_occlusion_intensity':0.,'bloom_intensity':0.,'motion_blur_amount':0.,'auto_exposure_method':u.AutoExposureMethod.AEM_MANUAL,'auto_exposure_apply_physical_camera_exposure':False,'auto_exposure_bias':0.}.items():settings.set_editor_property('override_'+k,True);settings.set_editor_property(k,v)
pp.set_editor_property('settings',settings)
u.get_editor_subsystem(u.UnrealEditorSubsystem).set_level_viewport_camera_info(cam.get_actor_location(),cam.get_actor_rotation())
# Compare imported render bounds with source world SI; reflection Y and metres -> centimetres.
source=json.loads((project/'Docs/ProcessPlant_20261110_LowPoly/visual_source.json').read_text(encoding='utf8'))
checks=[]
for obj in source['objects']:
 if obj['type']!='MESH' or obj['name'] not in inventory:continue
 a=inventory[obj['name']];c,e=a.get_actor_bounds(False);lo,hi=obj['bounds']
 expectedlo=[lo[0]*100,-hi[1]*100,lo[2]*100];expectedhi=[hi[0]*100,-lo[1]*100,hi[2]*100]
 actualmin=list((c-e).to_tuple());actualmax=list((c+e).to_tuple())
 delta=max(abs(a-b) for a,b in zip(actualmin+actualmax,expectedlo+expectedhi))
 checks.append(dict(name=obj['name'],error_cm=delta))
critical=[c for c in checks if any(k in c['name'] for k in ['UpperBody_Render','LowerCone_Render','MX-301_Render_Shell','MX-301_Paddle','RigidRoute_ID44','P201_LowRoute','P202_Reducer','RH201_110_Main_shell','WH101_Body_ID700'])]
assert max(c['error_cm'] for c in critical)<.1,sorted(critical,key=lambda c:-c['error_cm'])[:5]
(project/'Docs/ProcessPlant_20261110_LowPoly/scale_validation.json').write_text(json.dumps(dict(count=len(checks),max_error_cm=max(c['error_cm'] for c in checks),critical_max_error_cm=max(c['error_cm'] for c in critical),critical_count=len(critical),checks=checks),indent=2))
assert levels.save_current_level()
(project/'Docs/ProcessPlant_20261110_LowPoly/final_actor_inventory.json').write_text(json.dumps([dict(name=a.get_actor_label(),mesh=a.static_mesh_component.static_mesh.get_path_name(),transform=str(a.get_actor_transform())) for a in inventory.values()],indent=2))
(project/'Docs/ProcessPlant_20261110_LowPoly/level_integration.json').write_text(json.dumps(dict(level=level,parts=len(parts),frames=frames,cutaway=cuts,particles=test.get_editor_property('particle_count'),valves=[n for n in inventory if re.match(r'V101[A-F]_Disc$',n)],paddles=[n for n in inventory if n.startswith('MX-301_') and 'Paddle' in n]),indent=2))
