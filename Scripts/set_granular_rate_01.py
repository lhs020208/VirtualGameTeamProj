import unreal as u
import json
from pathlib import Path
editor=u.get_editor_subsystem(u.LevelEditorSubsystem)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
records=[]
for level in ['/Game/L_MX301_Test','/Game/DEM/Tests/L_MX301_BinaryMaterials']:
 assert editor.load_level(level)
 m=next(a for a in actors.get_all_level_actors() if a.get_class().get_name()=='PowderGranularMixer')
 names=['particle_count','time_step','contact_iterations','rpm','friction','binary_materials','enable_particle_rotation','rolling_friction']
 before={k:m.get_editor_property(k) for k in names}
 old=m.get_editor_property('target_rate')
 m.set_editor_property('target_rate',.1)
 assert editor.save_current_level()
 assert editor.load_level(level)
 m=next(a for a in actors.get_all_level_actors() if a.get_class().get_name()=='PowderGranularMixer')
 assert m.get_editor_property('target_rate')==.1
 assert before=={k:m.get_editor_property(k) for k in names}
 records.append(dict(level=level,previous_target=old,target=.1,unchanged_physics=before))
folder=Path('D:/GitHub/VirtualGameTeamProj/Docs/DEM/rate01_recheck');folder.mkdir(exist_ok=True)
(folder/'settings.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
