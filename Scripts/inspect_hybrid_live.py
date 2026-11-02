import unreal as u
import json
from pathlib import Path
world=u.EditorLevelLibrary.get_game_world()
actors=u.GameplayStatics.get_all_actors_of_class(world,u.Actor)
a=next(a for a in actors if a.get_class().get_name()=='PowderPlantTest')
a.export_snapshot()
groups=[g for g in a.get_components_by_class(u.InstancedStaticMeshComponent) if g.get_instance_count()==10]
assert len(groups)==3
p=groups[1].get_instance_transform(0,world_space=True).translation
camera=u.GameplayStatics.get_player_controller(world,0).get_view_target()
location=p+u.Vector(8,28,7)
camera.set_actor_location(location,False,False)
camera.set_actor_rotation(u.MathLibrary.find_look_at_rotation(location,p),False)
report={k:a.get_editor_property(k) for k in ['particle_count','physical_seconds','actual_rate','data_directory','fixed_dt','contact_iterations','error']}
Path('D:/GitHub/VirtualGameTeamProj/Docs/ProcessPlantPhysics/hybrid/optimization/live.json').write_text(json.dumps(report,indent=2))
u.log('HYBRID_LIVE_CAMERA_READY')
