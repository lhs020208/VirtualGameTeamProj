"""Read-only Blender export: revised plant visuals and separate physics evidence."""
import bpy,json,hashlib,re
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'SourceAssets/PlantRevision1125';DOC=ROOT/'Docs/PlantRevision1125'
OUT.mkdir(parents=True,exist_ok=True);DOC.mkdir(parents=True,exist_ok=True)
assert bpy.context.scene.unit_settings.scale_length==1
def family(o):return {o}|{v for c in o.children for v in family(c)}
rotating=family(bpy.data.objects['PIVOT_MX-301_SHAFT'])
visible=[o for o in bpy.context.visible_objects if o.type in {'MESH','CURVE','FONT','EMPTY'} and not o.hide_render
    and not any(w in o.name.lower() for w in ['collision','simulation','preview_ground','studio_floor'])
    and 'ClosedSeal' not in o.name and not o.get('export_exclude',False)
    and not any(c.get('export_exclude',False) for c in o.users_collection)]
visible=[o for o in visible if o.type!='MESH' or len(o.data.polygons)]
# FBX assigns materials per mesh. Resolve object overrides in this disposable
# Blender process so shared source datablocks cannot exchange their colours.
for o in visible:
    if o.type not in {'MESH','CURVE','FONT'}:continue
    materials=[s.material for s in o.material_slots]
    o.data=o.data.copy()
    for i,m in enumerate(materials):
        o.material_slots[i].link='DATA';o.material_slots[i].material=m
bpy.context.view_layer.update();graph=bpy.context.evaluated_depsgraph_get();records=[]
for o in sorted(visible,key=lambda o:o.name):
    if o.type=='MESH':
        e=o.evaluated_get(graph);m=e.to_mesh();v=[e.matrix_world@x.co for x in m.vertices];e.to_mesh_clear()
    else:v=[o.matrix_world@Vector(x) for x in o.bound_box]
    collections=[c.name for c in o.users_collection]
    background=o.name.startswith('ENV_') or any(c.startswith('ENV |') for c in collections)
    frame=not background and (any(c=='Frame' or c.startswith('SUPPORT |') or 'support pedestal' in c.lower() for c in collections)
        or bool(re.match(r'(MX301_Pedestal_|P10[12]_Rack|P201_Stand_|HLCM_|MX-301_(Base_Frame|Frame_|Support_|Rod_Foot|Vessel_Saddle)|H-101[A-F]_(Brace_|Bracket_|Foot_|Leg_|PneumaticFrame|PneumaticMount)|AIR101_(RiserPost|RiserClip|RiserBase|ClipFoot))',o.name)))
    records.append(dict(name=o.name,type=o.type,rotating=o in rotating,background=background,frame=frame,collections=collections,
        bounds=[[min(q[k] for q in v) for k in range(3)],[max(q[k] for q in v) for k in range(3)]],
        materials=[dict(name=m.name,color=list(m.diffuse_color)) for m in getattr(o.data,'materials',[]) if m]))
bpy.ops.object.select_all(action='DESELECT')
for o in visible:o.select_set(True)
bpy.ops.export_scene.fbx(filepath=str(OUT/'Plant.fbx'),use_selection=True,add_leaf_bones=False,bake_anim=False,axis_forward='-Y',axis_up='Z')
source=Path(bpy.data.filepath)
result=dict(source=str(source),sha256=hashlib.sha256(source.read_bytes()).hexdigest(),units='m',
    pivot_m=list(bpy.data.objects['PIVOT_MX-301_SHAFT'].matrix_world.translation),objects=records)
(DOC/'visual_source.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
ns={'__name__':'physics_export'}
exec(compile((ROOT/'Scripts/export_plant_physics_source.py').read_text(),'physics_export','exec'),ns)
ns['export'](ROOT/'Intermediate/Plant1125/Physics')
print('REVISION_EXPORTED',len(records),'background',sum(r['background'] for r in records),'frames',sum(r['frame'] for r in records),'pivot',result['pivot_m'])
