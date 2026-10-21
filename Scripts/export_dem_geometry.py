"""Export tagged BasicShapes/Cube components from the current editor world.

Does not save or change the level. In a Python commandlet, loads saved /Game/L_Main.
Run in the editor Python console: exec(open(..., encoding='utf-8').read()).
"""
import json
import math
from pathlib import Path
import unreal as u

def xyz(v): return [float(v.x),float(v.y),float(v.z)]

def component_box(component):
    mesh=component.get_editor_property('static_mesh')
    if not mesh or mesh.get_path_name()!='/Engine/BasicShapes/Cube.Cube':
        raise ValueError('DEM export supports only BasicShapes/Cube: '+component.get_path_name())
    return transform_box(component.get_world_transform())

def transform_box(t):
    centre=xyz(t.transform_location(u.Vector(0,0,0)))
    axes=[];half=[]
    for unit in ((1,0,0),(0,1,0),(0,0,1)):
        tip=xyz(t.transform_location(u.Vector(*unit)))
        delta=[tip[i]-centre[i] for i in range(3)]
        length=math.sqrt(sum(v*v for v in delta))
        if not math.isfinite(length) or length<=1e-8: raise ValueError('Degenerate box scale')
        axes.append([v/length for v in delta]);half.append(length*.5)
    # Unreal centimetres -> metres; preserve world axes, do not apply Euler guesses.
    return [v*.01 for v in centre],half,axes

def export():
    levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    if not world or 'L_Main' not in world.get_path_name():
        if '-run=pythonscript' not in u.SystemLibrary.get_command_line().lower(): raise RuntimeError('Open L_Main before exporting')
        if not levels.load_level('/Game/L_Main'): raise RuntimeError('Cannot load L_Main')
        world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
    tagged=sorted((a for a in actors if a.actor_has_tag('PowderRig')),key=lambda a:a.get_path_name())
    if not tagged: raise RuntimeError('No PowderRig actors')
    rows=[];manifest=[]
    for index,a in enumerate(tagged,1):
        components=a.get_components_by_class(u.StaticMeshComponent)
        if len(components)!=1: raise ValueError('Expected one cube component: '+a.get_path_name())
        centre,half,axes=component_box(components[0])
        values=[index,1,*centre,*half,*[v for axis in axes for v in axis]]
        if not all(math.isfinite(v) for v in values): raise ValueError('Nonfinite geometry')
        rows.append(' '.join(format(v,'.17g') for v in values))
        manifest.append(dict(id=index,label=a.get_actor_label(),path=a.get_path_name(),centre_m=centre,half_m=half,axes=axes))
    dest=Path(u.Paths.project_dir())/'Physics/PowderCore/fixtures'
    dest.mkdir(parents=True,exist_ok=True)
    (dest/'L_Main.boxes').write_text('POWDER_BOXES_SI 1 '+str(len(rows))+'\n'+'\n'.join(rows)+'\n',encoding='utf-8')
    (dest/'L_Main.manifest.json').write_text(json.dumps(dict(schema=1,world=world.get_path_name(),units='m',boxes=manifest),ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    groups=[[b['id'] for b in manifest if b['label'].startswith(prefix)] for prefix in ('Feed_Wall_','Feed_Funnel_','Tube_Wall_','Collector_')]
    (dest/'L_Main.tracking').write_text('POWDER_TRACKING_GROUPS 1\n'+'\n'.join(str(len(g))+' '+' '.join(map(str,g)) for g in groups)+'\n',encoding='utf-8')
    print('DEM_EXPORTED',len(rows),dest)

export()
