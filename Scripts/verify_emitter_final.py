import unreal as u
import json
from pathlib import Path
assert u.get_editor_subsystem(u.LevelEditorSubsystem).load_level('/Game/L_MX301_Test')
actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
m=next(a for a in actors if a.get_class().get_name()=='PowderGranularMixer')
assert m.get_editor_property('use_emitters')
sources=m.get_editor_property('emitters');assert len(sources)==2
for i,e in enumerate(sources):
 assert e.get_editor_property('particle_count')==25000
 assert e.get_editor_property('gpu_group')==i
 assert len(e.get_editor_property('initial_local_positions_meters'))==25000
 e.refresh_preview();assert e.get_editor_property('visible_instance_count')==2048
 c=e.get_components_by_class(u.InstancedStaticMeshComponent)[0]
 tint=c.get_material(0).get_vector_parameter_value('Tint')
 assert (tint.r>tint.b) if i==0 else (tint.b>tint.r)
m.reset_simulation();assert not m.get_editor_property('error')
assert m.get_editor_property('particle_count')==50000
Path('D:/GitHub/VirtualGameTeamProj/Docs/DEM/emitter_integration06/level_reload.json').write_text(json.dumps(dict(success=True,count=50000,groups=[25000,25000],preview_colors_verified=True,mass_kg=m.get_editor_property('charge_mass_kg')),indent=2))
