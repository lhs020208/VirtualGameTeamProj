from pathlib import Path
import json,unreal as u
root=Path('D:/GitHub/VirtualGameTeamProj');levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
assert levels.load_level('/Game/ProcessPlant_20261029/L_ProcessPlant_FlowTest')
a=next(a for a in actors.get_all_level_actors() if a.get_class().get_name()=='PowderPlantTest')
assert a.get_editor_property('data_directory')=='SourceAssets/ProcessPlantPhysics';a.reset_simulation();assert a.get_editor_property('particle_count')==30;assert not str(a.get_editor_property('error'))
groups=[]
for c in a.get_components_by_class(u.InstancedStaticMeshComponent):
 n=c.get_instance_count()
 if not n:continue
 mesh=c.get_editor_property('static_mesh');diameter_cm=max(mesh.get_bounds().box_extent.x,mesh.get_bounds().box_extent.y,mesh.get_bounds().box_extent.z)*2*c.get_instance_transform(0,False).scale3d.x
 assert n==10 and abs(diameter_cm-.4)<.00001
 groups.append(dict(count=n,diameter_cm=diameter_cm))
assert len(groups)==3
(root/'Docs/ProcessPlantPhysics/charge30/level_check.json').write_text(json.dumps(dict(count=30,groups=groups),indent=2));u.log('CHARGE30_VERIFIED')
