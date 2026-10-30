from pathlib import Path
import json,unreal as u
root=Path('D:/GitHub/VirtualGameTeamProj');specs=json.loads((root/'SourceAssets/ProcessPlantSections/sections.json').read_text());levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem);report=[]
for s in specs:
 path='/Game/ProcessPlant_20261029/'+s['level'];assert levels.load_level(path)
 tests=[a for a in actors.get_all_level_actors() if a.get_class().get_name()=='PowderPlantTest'];assert len(tests)==1
 a=tests[0];assert a.get_editor_property('section_test')==s['section'];assert a.get_editor_property('data_directory')=='SourceAssets/ProcessPlantSections/'+s['name'];a.reset_simulation();assert a.get_editor_property('particle_count')==3000;assert not str(a.get_editor_property('error'))
 report.append(dict(level=path,section=s['section'],actor_count=len(actors.get_all_level_actors()),particles=3000))
(root/'Docs/ProcessPlantPhysics/sections/levels.json').write_text(json.dumps(report,indent=2));u.log('SECTION_LEVELS_VERIFIED '+json.dumps(report))
