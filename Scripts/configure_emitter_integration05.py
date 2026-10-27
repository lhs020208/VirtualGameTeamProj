import unreal as u
import struct,json
from pathlib import Path
root=Path('D:/GitHub/VirtualGameTeamProj')
editor=u.get_editor_subsystem(u.LevelEditorSubsystem)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
backup='/Game/DEM/Tests/L_MX301_LegacyBaseline'
if not u.EditorAssetLibrary.does_asset_exist(backup):
 baseline=u.EditorAssetLibrary.duplicate_asset('/Game/L_MX301_Test',backup)
 assert baseline and u.EditorAssetLibrary.save_loaded_asset(baseline)
assert editor.load_level('/Game/L_MX301_Test')
mixers=[a for a in actors.get_all_level_actors() if a.get_class().get_name()=='PowderGranularMixer']
assert len(mixers)==1
m=mixers[0]
assert not m.get_editor_property('use_emitters'), 'Refuse to overwrite already configured level'
settings=u.load_asset('/Game/DEM/Settings/DA_DEMParticle_GPU_Default')
settings.set_editor_property('sphere_mesh',u.load_asset('/Game/DEM/SM_DEMSphere'))
settings.set_editor_property('material_override',None)
assert u.EditorAssetLibrary.save_loaded_asset(settings)
initial=list(struct.iter_unpack('ffff',(root/'Docs/DEM/emitter_integration05/reference_initial/initial.bin').read_bytes()))
cls=u.load_class(None,'/Game/DEM/Emitters/BP_DEMEmitter.BP_DEMEmitter_C')
emitters=[]
for group,label in [(0,'Powder_Red_Lower_25000'),(1,'Powder_Blue_Upper_25000')]:
 points=[p for p in initial if p[3]==group];assert len(points)==25000
 e=actors.spawn_actor_from_class(cls,m.get_actor_location(),m.get_actor_rotation())
 e.set_actor_label(label)
 e.set_editor_property('editor_preview',False)
 e.set_editor_property('particle_settings',settings)
 e.set_editor_property('particle_count',25000)
 e.set_editor_property('gpu_group',group)
 e.set_editor_property('initial_local_positions_meters',[u.Vector(p[0],p[1],p[2]) for p in points])
 e.set_editor_property('editor_preview',True)
 e.refresh_preview()
 assert e.get_editor_property('visible_instance_count')==2048
 emitters.append(e)
m.set_editor_property('use_emitters',True)
m.set_editor_property('emitters',emitters)
m.set_editor_property('particle_settings',settings)
m.set_editor_property('particle_count',50000)
assert m.validate_simulation_settings()
assert editor.save_current_level()
(root/'Docs/DEM/emitter_integration05/level.json').write_text(json.dumps({'level':'L_MX301_Test','baseline':backup,'groups':[25000,25000],'layout':'exact legacy initial positions, bottom red/top blue','mass_kg':m.get_editor_property('charge_mass_kg'),'preview_per_emitter':2048},indent=2))

