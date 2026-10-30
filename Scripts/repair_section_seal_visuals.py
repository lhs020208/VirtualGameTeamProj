import unreal as u
from pathlib import Path
import json
root=Path('D:/GitHub/VirtualGameTeamProj');base='/Game/ProcessPlant_20261029';levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
assert levels.load_level(base+'/L_ProcessPlant_FlowTest');snapshots={}
for a in actors.get_all_level_actors():
 if a.get_actor_label() in ['Test_PL_cuff_connection','Test_BV122_seal','Test_BV272_seal']:
  c=a.static_mesh_component;snapshots[a.get_actor_label()]=(a.get_actor_transform(),c.static_mesh,[c.get_material(i) for i in range(c.get_num_materials())])
for s in json.loads((root/'SourceAssets/ProcessPlantSections/sections.json').read_text()):
 if s['section'] not in [2,3]:continue
 assert levels.load_level(base+'/'+s['level']);inventory=actors.get_all_level_actors();names={a.get_actor_label() for a in inventory};controller=next(a for a in inventory if a.get_class().get_name()=='PlantVisibilityController');parts=list(controller.get_editor_property('parts'))
 for name,(transform,mesh,materials) in snapshots.items():
  if not name.startswith(tuple(s['visual_prefixes'])) or name in names:continue
  a=actors.spawn_actor_from_class(u.StaticMeshActor,transform.translation);a.set_actor_label(name);a.set_actor_transform(transform,False,False);a.static_mesh_component.set_static_mesh(mesh)
  for i,material in enumerate(materials):a.static_mesh_component.set_material(i,material)
  part=u.PlantVisualPart();part.set_editor_property('actor',a);part.set_editor_property('cutaway',True);part.set_editor_property('local_plane_normal',u.Vector(0,1,0));parts.append(part)
 controller.set_editor_property('parts',parts);controller.apply_display();assert levels.save_current_level()
u.log('SECTION_SEAL_VISUALS_PRESERVED '+str(list(snapshots)))
