"""Install interactive controls into the November plant using its own runtime data."""
import json,re
from pathlib import Path
import unreal as u
project=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir()))
root='/Game/ProcessPlant_20261103'
level=root+'/L_ProcessPlant_ParticleSetup'
levels=u.get_editor_subsystem(u.LevelEditorSubsystem); actors=u.get_editor_subsystem(u.EditorActorSubsystem)
assert levels.load_level(level)
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
inventory={a.get_actor_label():a for a in actors.get_all_level_actors() if isinstance(a,u.StaticMeshActor)}
assert len(inventory)==2254
pclass=u.load_class(None,'/Script/VirtualGameTeamProj.PowderPlantTest')
dclass=u.load_class(None,'/Script/VirtualGameTeamProj.PlantVisibilityController')
def singleton(cls):
 matches=[a for a in actors.get_all_level_actors() if a.get_class()==cls]
 assert len(matches)<=1
 return matches[0] if matches else actors.spawn_actor_from_class(cls,u.Vector())
test=singleton(pclass);test.set_actor_label('Plant_Pressure_Grains_20261103')
test.set_editor_property('data_directory','SourceAssets/ProcessPlant_20261103/Simulation')
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
 frame=bool(re.match(r'(P10[12]_Rack|P201_Stand_|P201_(Horizontal|Riser)SupportClamp|RH_Frame_|HLCM_|WH101_(Frame_|Foot_Anchor|Lower_Frame_Brace)|MX-301_(Base_Frame|Frame_|Support_|Rod_Foot|Vessel_Saddle)|H-101[A-F]_(Brace_|Bracket_|Foot_|Leg_|PneumaticFrame|PneumaticMount)|AIR101_(RiserPost|RiserClip|RiserBase|ClipFoot))',name))
 ref=None
 if not frame:
  if re.match(r'H-101[A-F]_(UpperBody|LowerCone|ClampGasket|ClampRing|Rim_|Label|Nameplate)',name):ref=name[:6]+'_UpperBody_Render'
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
  for i in range(comp.get_num_materials()):
   mat=comp.get_material(i);key=mat.get_name() if mat else ''
   if key not in materials:
    path=cutroot+'/Cutaway/MI_'+key
    materials[key]=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else fallback
   comp.set_material(i,materials[key])
  cuts+=1
 parts.append(part)
cam=next(a for a in actors.get_all_level_actors() if a.get_actor_label()=='Plant_OverviewCamera')
cam.set_actor_location(u.Vector(570,550,430),False,False);cam.set_actor_rotation(u.MathLibrary.find_look_at_rotation(cam.get_actor_location(),u.Vector(100,0,195)),False)
controller.parts=parts;controller.overview_camera=cam;controller.show_cutaway();controller.set_frames_visible(False)
world.get_world_settings().set_editor_property('default_game_mode',u.load_class(None,'/Script/VirtualGameTeamProj.PowderBulkGameMode'))
for a in actors.get_all_level_actors():
 if a.get_actor_label() in ['Initial_Powder_A','Initial_Powder_B']:actors.destroy_actor(a)
assert levels.save_current_level()
(project/'Docs/ProcessPlant_20261103/level_integration.json').write_text(json.dumps(dict(level=level,parts=len(parts),frames=frames,cutaway=cuts,particles=test.get_editor_property('particle_count'),valves=[n for n in inventory if re.match(r'V101[A-F]_Disc$',n)],paddles=[n for n in inventory if n.startswith('MX-301_') and 'Paddle' in n]),indent=2))
