"""Apply a completed revision import to the current level, preserving control actors.

Run once after import_plant_revision1125.py. The saved old actor paths make
replacement ownership explicit. Background geometry is neither a visual control
part nor a physics input. The original .blend is never saved or edited.
"""
import unreal as u,json,re
from pathlib import Path
project=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir()))
doc=project/'Docs/PlantRevision1125';tmp=project/'Intermediate/Plant1125'
root='/Game/ProcessPlant_20261111_LowPoly';dest=root+'/Revision1125'
es=u.get_editor_subsystem(u.EditorActorSubsystem);ls=u.get_editor_subsystem(u.LevelEditorSubsystem)
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name()==root+'/L_ProcessPlant_LowPoly_1111.L_ProcessPlant_LowPoly_1111'
source=json.loads((doc/'visual_source.json').read_text(encoding='utf8'))
records={o['name'].replace('.','_').replace(' ','_'):o for o in source['objects']}
oldpaths=set(json.loads((tmp/'old_actors.json').read_text()))
allactors=es.get_all_level_actors()
test=next(a for a in allactors if a.get_class().get_name()=='PowderPlantTest')
control=next(a for a in allactors if a.get_class().get_name()=='PlantVisibilityController')
savedmode=control.view_mode;savedframes=control.show_frames
new={a.get_actor_label():a for a in allactors if a.get_path_name() not in oldpaths and isinstance(a,u.StaticMeshActor)}
assert len(new)==sum(o['type'] in ['MESH','CURVE','FONT'] for o in source['objects'])
assert all(n in records for n in new)
simple=u.load_asset(root+'/Materials/M_PlantSimple');cut=u.load_asset(root+'/Materials/M_PlantCutaway')
assert simple and cut
assert simple.get_editor_property('shading_model')==u.MaterialShadingModel.MSM_UNLIT
assert cut.get_editor_property('shading_model')==u.MaterialShadingModel.MSM_UNLIT
parts=[];materials={};checks=[];background=[];rotors=[];frames=[]
for name,a in new.items():
 o=records[name];c=a.static_mesh_component;ref=None
 if not o['frame'] and not o['background']:
  if re.match(r'H-101[A-F]_(UpperBody|LowerCone|ClampGasket|ClampRing|Rim_|Label|Nameplate|Lid)',name):ref=name[:6]+'_UpperBody_Render'
  elif re.match(r'WH101_(Body|Cone|Inlet|Top|Separate|Inspection|Outlet)',name):ref='WH101_Body_ID700'
  elif name.startswith('RH201_') and not name.startswith('RH201_Filter_'):ref='RH201_110_Main_shell'
  elif re.match(r'MX-301_(Render_Shell|Left_EndPlate|Right_EndPlate|Lid_|Handle_|Inlet_)',name):ref='MX-301_Render_Shell'
  elif re.match(r'(P10[12][A-F]_|P201_(Expansion|LowRoute|VT_)|P202_|PL_|FH122S_|CL122_|V101[A-F]_(Body|Union_)|BV(122|272)_(PIPE_|11[0-6]_)|ASSUMED_(Transition_|VT_(BoreBody|EndFlange|Housing)))',name):ref=name
 part=u.PlantVisualPart();part.actor=a;part.frame=o['frame'];part.cutaway=bool(ref)
 if ref:
  assert ref in new,ref
  origin,_=new[ref].get_actor_bounds(False);t=c.get_world_transform()
  part.local_plane_origin=t.inverse_transform_location(origin)
  part.local_plane_normal=t.rotation.unrotate_vector(u.Vector(0,1,0))*t.scale3d
 if not o['background']:parts.append(part)
 else:background.append(name)
 if o['frame']:frames.append(name)
 tags=['PlantRevision1125']
 if o['background']:tags.append('PlantBackgroundDisplayOnly')
 elif o['rotating']:tags.append('PlantMixerRotor');rotors.append(name)
 elif name.startswith(('MX-301_','ST-301_')):tags.append('PlantMixerStatic')
 a.set_editor_property('tags',[u.Name(t) for t in tags])
 a.set_folder_path('Background' if o['background'] else 'Process/Frames' if o['frame'] else 'Process/Equipment')
 if o['rotating']:c.set_mobility(u.ComponentMobility.MOVABLE)
 c.set_collision_enabled(u.CollisionEnabled.NO_COLLISION);c.set_editor_property('generate_overlap_events',False);c.set_cast_shadow(False)
 c.set_editor_property('affect_distance_field_lighting',False)
 # Persist the profile too: PostEditChange restores the profile after material
 # edits, so SetCollisionEnabled alone is insufficient for imported actors.
 c.set_collision_profile_name('NoCollision')
 for i in range(c.get_num_materials()):
  src=o['materials'][min(i,len(o['materials'])-1)] if o['materials'] else {'name':'grey'}
  label=src['name'].lower()
  kind='blue' if 'blue' in label else 'black' if any(t in label for t in ['black','dark','rubber']) else 'bronze' if any(t in label for t in ['bronze','brass']) else 'yellow' if any(t in label for t in ['yellow','marking','safety']) else 'white' if 'white' in label else 'grey'
  if o['background'] and kind=='grey':kind='background'
  key=(kind,bool(ref))
  if key not in materials:
   assetname='MI_'+kind+('_Cut' if ref else '_Solid');path=dest+'/Materials/'+assetname
   mat=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else u.AssetToolsHelpers.get_asset_tools().create_asset(assetname,dest+'/Materials',u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
   u.MaterialEditingLibrary.set_material_instance_parent(mat,cut if ref else simple)
   tint={'blue':(.01,.25,.5,1),'black':(.025,.04,.05,1),'bronze':(.38,.25,.1,1),'white':(.65,.68,.7,1),'grey':(.4,.45,.48,1),'background':(.25,.28,.3,1),'yellow':(.55,.4,.035,1)}[kind]
   u.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mat,'Tint',u.LinearColor(*tint))
   u.MaterialEditingLibrary.update_material_instance(mat);u.EditorAssetLibrary.save_loaded_asset(mat);materials[key]=mat
  c.set_material(i,materials[key])
 if o['type']=='MESH':
  centre,extent=a.get_actor_bounds(False);lo,hi=o['bounds']
  expect=[lo[0]*100,-hi[1]*100,lo[2]*100,hi[0]*100,-lo[1]*100,hi[2]*100]
  actual=list((centre-extent).to_tuple())+list((centre+extent).to_tuple())
  checks.append(dict(name=name,error_cm=max(abs(x-y) for x,y in zip(actual,expect))))
assert len(rotors)==47
critical=[c for c in checks if c['name'].startswith('MX-301_') or 'RigidRoute_ID44' in c['name']]
assert max(c['error_cm'] for c in critical)<.1,sorted(critical,key=lambda c:-c['error_cm'])[:10]
# Only imported geometry belongs to this replacement. Cameras, controller, solver,
# post-process and lighting actors keep their identities and existing UI bindings.
oldroot=next(a for a in allactors if a.get_path_name() in oldpaths and a.get_name()=='RootNode')
oldvisual=[]
for a in allactors:
 if a.get_path_name() not in oldpaths:continue
 parent=a
 while parent and parent!=oldroot:parent=parent.get_attach_parent_actor()
 if parent==oldroot:oldvisual.append(a)
assert len(oldvisual)>2000
control.set_editor_property('parts',parts)
for a in reversed(oldvisual):es.destroy_actor(a)
control.set_view_mode(savedmode);control.set_frames_visible(savedframes)
test.set_editor_property('data_directory','SourceAssets/PlantRevision1125/Simulation')
test.set_editor_property('compute_backend',u.PlantComputeBackend.OPEN_CL)
test.set_editor_property('demo_realtime',False)
test.set_editor_property('mixer_pivot_m',u.Vector(3,0,.9))
test.set_editor_property('mixer_bounds_min_m',u.Vector(2.25,-.3225,.5775))
test.set_editor_property('mixer_bounds_max_m',u.Vector(3.75,.3225,1.225))
test.reset_simulation()
assert not str(test.get_editor_property('error')),str(test.get_editor_property('error'))
assert test.get_editor_property('particle_count')==20000
assert u.EditorAssetLibrary.save_directory(dest,only_if_is_dirty=True,recursive=True)
assert ls.save_current_level()
report=dict(level=world.get_path_name(),source_sha256=source['sha256'],backend=str(test.get_editor_property('compute_backend')),
 parts=len(parts),background=background,frames=frames,rotating_parts=rotors,cutaway=sum(p.cutaway for p in parts),
 materials='Unlit parents, no cast shadows / distance-field lighting / overlap / Chaos collision',
 particles=test.get_editor_property('particle_count'),pivot_m=[3,0,.9],previous_visual_actors_replaced=len(oldvisual),bounds_checks=checks)
(doc/'level_integration.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
print('REVISION_APPLIED',len(new),len(background),len(parts),len(rotors))
