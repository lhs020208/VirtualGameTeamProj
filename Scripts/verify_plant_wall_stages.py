import unreal as u
import json
from pathlib import Path
world=u.EditorLevelLibrary.get_game_world()
actors=u.GameplayStatics.get_all_actors_of_class(world,u.Actor)
a=next(a for a in actors if a.get_class().get_name()=='PowderPlantTest')
a.export_snapshot()
report={k:a.get_editor_property(k) for k in ['particle_count','physical_seconds','actual_rate','data_directory','fixed_dt','contact_iterations','error']}
report['level']=world.get_name()
report['instance_counts']=[g.get_instance_count() for g in a.get_components_by_class(u.InstancedStaticMeshComponent)]
Path('D:/GitHub/VirtualGameTeamProj/Docs/ProcessPlantPhysics/hybrid/wall_stages/live.json').write_text(json.dumps(report,indent=2))
u.log('PLANT_WALL_STAGES_LIVE '+json.dumps(report))
