import unreal as u,json
from pathlib import Path
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
r={'world':world.get_path_name(),'enums':{n:[x for x in dir(getattr(u,n)) if x.isupper()] for n in ['DynamicGlobalIlluminationMethod','ReflectionMethod','AutoExposureMethod']},'postprocess_fields':[x for x in dir(u.PostProcessSettings()) if any(w in x for w in ['exposure','bloom','motion_blur','reflection_method','global_illumination_method','ambient_occlusion'])],'actors':[]}
for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
 r['actors'].append({'name':a.get_actor_label(),'class':a.get_class().get_name(),'meshes':[{'name':c.get_name(),'slots':c.get_num_materials(),'mesh':c.static_mesh.get_path_name() if c.static_mesh else None} for c in a.get_components_by_class(u.StaticMeshComponent)]})
Path('D:/GitHub/VirtualGameTeamProj/Saved/visual_inspect.json').write_text(json.dumps(r,indent=2))
