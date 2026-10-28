"""Create a separate uncalibrated binary-material test, preserve L_MX301_Test."""
import unreal as u
import json,struct,math
from pathlib import Path
root=Path('D:/GitHub/VirtualGameTeamProj')
dest='/Game/DEM/Tests/L_MX301_BinaryMaterials'
assert not u.EditorAssetLibrary.does_asset_exist(dest),'Refuse to overwrite test level'
assert u.EditorAssetLibrary.duplicate_asset('/Game/L_MX301_Test',dest)
editor=u.get_editor_subsystem(u.LevelEditorSubsystem)
assert editor.load_level(dest)
actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
m=next(a for a in actors if a.get_class().get_name()=='PowderGranularMixer')
emitters=sorted(m.get_editor_property('emitters'),key=lambda e:e.get_editor_property('gpu_group'))
assert len(emitters)==2
points=list(struct.iter_unpack('ffff',(root/'Docs/DEM/emitter_integration05/reference_initial/initial.bin').read_bytes()))
assert len(points)==50000
materials=[dict(radius=.0063647064,density=900,mu=.3,wall=.25),dict(radius=.005,density=1500,mu=.5,wall=.45)]
masses=[4*math.pi/3*x['radius']**3*x['density'] for x in materials]
na=round(50000*.2*masses[1]/(.8*masses[0]+.2*masses[1]))
counts=[na,50000-na]
for g,e in enumerate(emitters):
 path=f'/Game/DEM/Settings/DA_VirtualPowder_{"A" if g==0 else "B"}'
 assert not u.EditorAssetLibrary.does_asset_exist(path)
 s=u.EditorAssetLibrary.duplicate_asset('/Game/DEM/Settings/DA_DEMParticle_GPU_Default',path)
 p=materials[g]
 for name,value in [('radius_meters',p['radius']),('density_kg_per_cubic_meter',p['density']),('particle_friction',p['mu']),('wall_friction',p['wall']),('substance_name',f'Virtual {"A" if g==0 else "B"} (not a pharmaceutical)'),('property_source','Explicit test assumptions; not measured or calibrated'),('source_diameter_meters',0.)]:s.set_editor_property(name,value)
 assert u.EditorAssetLibrary.save_loaded_asset(s)
 selected=points[:na] if g==0 else points[na:]
 e.set_editor_property('editor_preview',False)
 e.set_editor_property('particle_settings',s)
 e.set_editor_property('particle_count',len(selected))
 e.set_editor_property('initial_local_positions_meters',[u.Vector(*p[:3]) for p in selected])
 e.set_actor_label(f'Virtual_{"A_red_lower" if g==0 else "B_blue_upper"}_{len(selected)}')
 e.set_editor_property('editor_preview',True);e.refresh_preview()
m.set_editor_property('particle_settings',None)
m.set_editor_property('binary_materials',True)
m.set_editor_property('material_optimizations',True)
m.set_editor_property('enable_particle_rotation',True)
m.set_editor_property('rolling_friction',.02)
m.set_editor_property('cross_material_friction',.35)
m.set_editor_property('target_rate',.1)
m.reset_simulation()
assert not m.get_editor_property('error'),m.get_editor_property('error')
assert editor.save_current_level()
out={'level':dest,'calibrated':False,'cohesion':False,'counts':counts,'materials':materials,'mass_kg':sum(n*v for n,v in zip(counts,masses)),'actual_mass_fraction_a':counts[0]*masses[0]/sum(n*v for n,v in zip(counts,masses)),'rotation':True,'rolling_friction':.02,'target_rate':.1,'baseline_unchanged':True}
(root/'Docs/DEM/material_extension/preset.json').write_text(json.dumps(out,indent=2),encoding='utf-8')
