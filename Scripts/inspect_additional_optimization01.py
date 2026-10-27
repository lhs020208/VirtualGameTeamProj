"""Read saved assets without editing/saving the user's level."""
import unreal as u
import json,struct,hashlib
from pathlib import Path
def transform(t):
 q=t.rotation;p=t.translation;s=t.scale3d
 return dict(position_cm=[p.x,p.y,p.z],rotation_xyzw=[q.x,q.y,q.z,q.w],scale=[s.x,s.y,s.z])
root=Path(__file__).resolve().parents[1]
out=root/'Docs/DEM/additional_optimization01'
assert u.get_editor_subsystem(u.LevelEditorSubsystem).load_level('/Game/L_MX301_Test')
actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
ms=[a for a in actors if a.get_class().get_name()=='PowderGranularMixer'];assert len(ms)==1
m=ms[0]
keys=['use_emitters','particle_count','time_step','contact_iterations','solver_batch_size','test_mixer_asset','friction','density_kg_per_cubic_meter','rpm','running','target_rate','gate_open','automatic_sequence','mix_until_seconds']
report={'level':'/Game/L_MX301_Test','mixer':{k:m.get_editor_property(k) for k in keys},'mixer_transform':transform(m.get_actor_transform()),'emitters':[],'engine_version':u.SystemLibrary.get_engine_version()}
assert report['mixer']['use_emitters'] and report['mixer']['test_mixer_asset']
for e in m.get_editor_property('emitters'):
 s=e.get_editor_property('particle_settings')
 fields=['gpu_approximation','radius_meters','density_kg_per_cubic_meter','particle_friction','wall_friction','normal_stiffness','tangential_stiffness','normal_damping_ratio','tangential_damping_ratio']
 pts=e.get_editor_property('initial_local_positions_meters')
 layout=b''.join(struct.pack('<ddd',p.x,p.y,p.z) for p in pts)
 v=e.get_editor_property('initial_linear_velocity');w=e.get_editor_property('initial_angular_velocity')
 report['emitters'].append(dict(label=e.get_actor_label(),group=e.get_editor_property('gpu_group'),count=e.get_editor_property('particle_count'),transform=transform(e.get_actor_transform()),velocity=[v.x,v.y,v.z],spin=[w.x,w.y,w.z],layout_count=len(pts),layout_sha256=hashlib.sha256(layout).hexdigest(),asset=s.get_path_name(),settings={k:s.get_editor_property(k) for k in fields},mesh=s.get_editor_property('sphere_mesh').get_path_name(),material=str(s.get_editor_property('material_override'))))
assert [e['count'] for e in report['emitters']]==[25000,25000]
assert [e['group'] for e in report['emitters']]==[0,1]
assert all(e['velocity']==[0,0,0] and e['spin']==[0,0,0] for e in report['emitters'])
(out/'saved_scene.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
u.log('BASELINE_SCENE_INSPECTION_OK')
