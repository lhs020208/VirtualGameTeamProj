import unreal as u,json
from pathlib import Path
u.get_editor_subsystem(u.LevelEditorSubsystem).load_level('/Game/ProcessPlant_20261029/L_ProcessPlant_Assembly')
r=[]
for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
 if 'ClampLever' not in a.get_actor_label():continue
 c=a.get_component_by_class(u.StaticMeshComponent)
 if not c:continue
 m=c.static_mesh.get_static_mesh_description(0)
 positions=[list(c.get_world_transform().transform_location(m.get_vertex_position(u.VertexID(i))).to_tuple()) for i in range(m.get_vertex_count()) if m.is_vertex_valid(u.VertexID(i))]
 r.append(dict(name=a.get_actor_label(),vertices=positions))
Path('D:/GitHub/VirtualGameTeamProj/Docs/ProcessPlant_20261029/lever_geometry.json').write_text(json.dumps(r),encoding='utf-8')