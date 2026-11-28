"""Editor verification, including independent visibility modes and display-only background."""
import unreal as u,json,hashlib
from pathlib import Path
p=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir()));doc=p/'Docs/PlantRevision1125'
source=json.loads((doc/'visual_source.json').read_text(encoding='utf8'))
records={o['name'].replace('.','_').replace(' ','_'):o for o in source['objects']}
es=u.get_editor_subsystem(u.EditorActorSubsystem);actors=es.get_all_level_actors()
test=next(a for a in actors if a.get_class().get_name()=='PowderPlantTest')
control=next(a for a in actors if a.get_class().get_name()=='PlantVisibilityController')
visuals=[a for a in actors if isinstance(a,u.StaticMeshActor)]
assert len(visuals)==2515
assert all('PlantRevision1125' in [str(t) for t in a.tags] for a in visuals)
assert str(test.get_editor_property('compute_backend'))=='<PlantComputeBackend.OPEN_CL: 0>'
assert test.get_editor_property('data_directory')=='SourceAssets/PlantRevision1125/Simulation'
assert test.get_editor_property('particle_count')==20000 and not test.get_editor_property('demo_realtime')
assert not str(test.get_editor_property('error'))
background=[a for a in visuals if a.actor_has_tag('PlantBackgroundDisplayOnly')]
rotors=[a for a in visuals if a.actor_has_tag('PlantMixerRotor')]
assert len(background)==247 and len(rotors)==47
particle_groups=[]
for c in test.get_components_by_class(u.InstancedStaticMeshComponent):
 if not c.get_instance_count():continue
 m=c.get_material(0)
 while isinstance(m,u.MaterialInstance):m=m.get_editor_property('parent')
 assert m.get_editor_property('shading_model')==u.MaterialShadingModel.MSM_UNLIT
 assert not c.get_editor_property('cast_shadow')
 particle_groups.append(c.get_instance_count())
assert particle_groups==[10000,10000]
post=next(a for a in actors if a.get_actor_label()=='Plant_SimpleDisplay').get_editor_property('settings')
assert post.get_editor_property('dynamic_global_illumination_method')==u.DynamicGlobalIlluminationMethod.NONE
assert post.get_editor_property('reflection_method')==u.ReflectionMethod.NONE
for prop in ['bloom_intensity','motion_blur_amount','ambient_occlusion_intensity']:assert post.get_editor_property(prop)==0
parts=list(control.parts);owned={p.actor.get_path_name() for p in parts}
assert len(owned)==2268 and not any(a.get_path_name() in owned for a in background)
checks=[];materialnames=set()
for a in visuals:
 c=a.static_mesh_component;o=records[a.get_actor_label()]
 assert c.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION
 assert not c.get_editor_property('cast_shadow') and not c.get_editor_property('generate_overlap_events')
 for i in range(c.get_num_materials()):
  m=c.get_material(i);materialnames.add(m.get_path_name())
  while isinstance(m,u.MaterialInstance):m=m.get_editor_property('parent')
  assert m.get_editor_property('shading_model')==u.MaterialShadingModel.MSM_UNLIT
 if o['type']=='MESH':
  centre,extent=a.get_actor_bounds(False);lo,hi=o['bounds']
  expected=[lo[0]*100,-hi[1]*100,lo[2]*100,hi[0]*100,-lo[1]*100,hi[2]*100]
  actual=list((centre-extent).to_tuple())+list((centre+extent).to_tuple())
  checks.append(dict(name=a.get_actor_label(),error_cm=max(abs(x-y) for x,y in zip(actual,expected))))
critical=[x for x in checks if x['name'].startswith('MX-301_') or 'RigidRoute_ID44' in x['name']]
assert max(x['error_cm'] for x in critical)<.1
mode=control.view_mode;frames=control.show_frames
physics=p/test.get_editor_property('data_directory')/'runtime.plant';before=hashlib.sha256(physics.read_bytes()).hexdigest()
try:
 for view in [u.PlantViewMode.FULL,u.PlantViewMode.CUTAWAY,u.PlantViewMode.HIDDEN]:
  for show in [True,False]:
   control.set_view_mode(view);control.set_frames_visible(show)
   for part in parts:
    expected=show if part.frame else view!=u.PlantViewMode.HIDDEN
    assert part.actor.static_mesh_component.is_visible()==expected
    assert part.actor.static_mesh_component.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION
   assert all(a.static_mesh_component.is_visible() for a in background)
finally:
 control.set_view_mode(mode);control.set_frames_visible(frames)
assert before==hashlib.sha256(physics.read_bytes()).hexdigest()
assert u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
report=dict(level=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world().get_path_name(),
 source_sha256=source['sha256'],backend='OpenCL',particles=test.get_editor_property('particle_count'),
 visual_mesh_actors=len(visuals),process_parts=len(parts),background_count=len(background),
 frames=sum(x.frame for x in parts),cutaway=sum(x.cutaway for x in parts),rotating_parts=[a.get_actor_label() for a in rotors],
 all_materials_unlit=True,particle_groups_unlit=particle_groups,postprocess_gi_reflection_bloom_motion_blur_ao_disabled=True,
 materials=sorted(materialnames),visibility_combinations_passed=6,background_collision='NoCollision; excluded from solver export and display controller',
 physics_hash_unchanged_by_visibility=True,bounds_checks=checks,critical_max_error_cm=max(x['error_cm'] for x in critical))
(doc/'level_integration.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
print('REVISED_LEVEL_VERIFIED',report['level'],len(background),len(rotors))
