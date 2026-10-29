"""Set up render-only controls on the imported plant, without starting a game world."""
import json, re
from pathlib import Path
import unreal as u

ROOT='/Game/ProcessPlant_20261029'
SOURCE_LEVEL=ROOT+'/L_ProcessPlant_Assembly'
LEVEL=ROOT+'/L_ProcessPlant_Assembly_Display'
OUT=Path('D:/GitHub/VirtualGameTeamProj/Docs/ProcessPlant_20261029')
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
u.AssetRegistryHelpers.get_asset_registry().search_all_assets(synchronous_search=True)
assert not u.EditorAssetLibrary.does_asset_exist(LEVEL),'Refusing to overwrite display level'
assert u.EditorAssetLibrary.duplicate_asset(SOURCE_LEVEL,LEVEL)
assert levels.load_level(LEVEL)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
cls=u.load_class(None,'/Script/VirtualGameTeamProj.PlantVisibilityController')
assert cls
assert not any(a.get_class()==cls for a in actors.get_all_level_actors()),'Display controller already installed'
inventory={a.get_actor_label():a for a in actors.get_all_level_actors() if a.get_component_by_class(u.StaticMeshComponent)}
assert len(inventory)==1717

def is_frame(n):
    return bool(re.match(r'(P101_Rack|P201_Stand_|P201_(Horizontal|Riser)SupportClamp|RH_Frame_|HLCM_|WH101_(Frame_|Foot_Anchor|Lower_Frame_Brace)|MX-301_(Base_Frame|Frame_|Support_|Rod_Foot|Vessel_Saddle)|MX-301_Drive_(Foot|Post_|BaseExtension|FrameTie|MountPlate|GuardBracket)|H-101[ABC]_(Brace_|Bracket_|Foot_|Leg_|PneumaticFrame|PneumaticMount|PneumaticConePad)|AIR101_(RiserPost|RiserClip|RiserBase|ClipFoot)|AIR_EQ01_(Support_|Saddle_bolt|Pipe_support)|AIR_VG_(Extended_support|Rail_bolt|Receiver_mount|Wall_bolt))',n))

def cut_reference(n):
    if is_frame(n):return None
    if n.startswith('MX-301_'):
        if re.match(r'MX-301_(Render_Shell|Left_EndPlate|Right_EndPlate|Lid_|Handle_|Inlet_|ID_|Interlock_Lid)',n):return 'MX-301_Render_Shell'
        return None  # Shafts, paddle arms, paddles and drive remain visible.
    if re.match(r'H-101[ABC]_(UpperBody|LowerCone|ClampGasket|ClampRing|Rim_|Label|Nameplate)',n):return n[:6]+'_UpperBody_Render'
    if n.startswith('WH101_') and not n.startswith('WH101_VG_'):return 'WH101_Body_ID700'
    if n.startswith('RH201_') and not n.startswith('RH201_Filter_'):return 'RH201_110_Main_shell'
    if re.match(r'(P101[ABC]_|P201_(Expansion|LowRoute|VT_)|P202_|PL_|FH122S_|CL122_)',n):return n
    if re.match(r'V101[ABC]_(Body|Union_)',n):return n[:5]+'_Body'
    if re.match(r'BV(122|272)_(PIPE_|11[0-6]_)',n):return n[:5]+'_110_Split_body_half'
    if re.match(r'ASSUMED_(Transition_|VT_(BoreBody|EndFlange|Housing)|WH101_(OUT_|B_))',n):return n
    return None

# Keep the cheap opaque master for every other mesh. Only sectioned shells use masked shading.
material_path=ROOT+'/Materials/M_PlantCutaway'
assert not u.EditorAssetLibrary.does_asset_exist(material_path)
m=u.EditorAssetLibrary.duplicate_asset(ROOT+'/Materials/M_PlantSimple',material_path)
m.set_editor_property('blend_mode',u.BlendMode.BLEND_MASKED)
m.set_editor_property('two_sided',True)
def node(cls,x,y):return u.MaterialEditingLibrary.create_material_expression(m,cls,x,y)
def link(a,b,pin):assert u.MaterialEditingLibrary.connect_material_expressions(a,'',b,pin)
def scalar(name,index,default):
    p=node(u.MaterialExpressionScalarParameter,-900,650+index*70)
    p.set_editor_property('parameter_name',name);p.set_editor_property('default_value',default)
    p.set_editor_property('use_custom_primitive_data',True);p.set_editor_property('primitive_data_index',index)
    return p
active=scalar('CutEnabled',0,0.)
distance=scalar('CutDistanceCm',4,0.)
normal=node(u.MaterialExpressionVectorParameter,-900,850)
normal.set_editor_property('parameter_name','CutNormal')
normal.set_editor_property('default_value',u.LinearColor(0,1,0,0))
normal.set_editor_property('use_custom_primitive_data',True);normal.set_editor_property('primitive_data_index',1)
position=node(u.MaterialExpressionWorldPosition,-900,1050)
dot=node(u.MaterialExpressionDotProduct,-600,850);link(position,dot,'A');link(normal,dot,'B')
one=node(u.MaterialExpressionConstant,-600,1050);one.set_editor_property('r',1.)
zero=node(u.MaterialExpressionConstant,-600,1120);zero.set_editor_property('r',0.)
section=node(u.MaterialExpressionIf,-350,850)
link(dot,section,'A');link(distance,section,'B')
link(zero,section,'A > B');link(one,section,'A == B');link(one,section,'A < B')
mask=node(u.MaterialExpressionLinearInterpolate,-100,850)
link(one,mask,'A');link(section,mask,'B');link(active,mask,'Alpha')
assert u.MaterialEditingLibrary.connect_material_property(mask,'',u.MaterialProperty.MP_OPACITY_MASK)
u.MaterialEditingLibrary.recompile_material(m)
assert u.EditorAssetLibrary.save_loaded_asset(m)

materials={}
parts=[];rows=[]
for name,actor in inventory.items():
    comp=actor.get_component_by_class(u.StaticMeshComponent)
    reference=cut_reference(name)
    part=u.PlantVisualPart()
    part.set_editor_property('actor',actor)
    part.set_editor_property('frame',is_frame(name))
    part.set_editor_property('cutaway',reference is not None)
    plane=None
    if reference:
        assert reference in inventory,reference
        origin,_=inventory[reference].get_actor_bounds(False)
        transform=comp.get_world_transform()
        local_origin=transform.inverse_transform_location(origin)
        # Covector transform: local normal = scale * inverse rotation * world normal.
        local_normal=transform.rotation.unrotate_vector(u.Vector(0,1,0))*transform.scale3d
        part.set_editor_property('local_plane_origin',local_origin)
        part.set_editor_property('local_plane_normal',local_normal)
        plane=list(origin.to_tuple())
        for i in range(comp.get_num_materials()):
            original=comp.get_material(i)
            assert original
            key=original.get_path_name()
            if key not in materials:
                instance=u.EditorAssetLibrary.duplicate_asset(key,ROOT+'/Materials/Cutaway/MI_'+original.get_name())
                assert instance
                tint=u.MaterialEditingLibrary.get_material_instance_vector_parameter_value(original,'Tint')
                u.MaterialEditingLibrary.set_material_instance_parent(instance,m)
                u.MaterialEditingLibrary.set_material_instance_vector_parameter_value(instance,'Tint',tint)
                u.MaterialEditingLibrary.update_material_instance(instance)
                assert u.EditorAssetLibrary.save_loaded_asset(instance)
                materials[key]=instance
            comp.set_material(i,materials[key])
    parts.append(part)
    rows.append(dict(name=name,frame=is_frame(name),cutaway=reference is not None,plane_reference=reference,world_plane_origin=plane))

controller=actors.spawn_actor_from_class(cls,u.Vector())
controller.set_actor_label('Plant_DisplayControls')
controller.set_editor_property('parts',parts)
camera=actors.spawn_actor_from_class(u.CameraActor,u.Vector(570,470,430),u.MathLibrary.find_look_at_rotation(u.Vector(570,470,430),u.Vector(150,-50,200)))
camera.set_actor_label('Plant_OverviewCamera')
controller.set_editor_property('overview_camera',camera)
controller.show_full();controller.set_frames_visible(True)
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
# This game mode supplies only the existing editor-style camera controller, no powder actors.
world.get_world_settings().set_editor_property('default_game_mode',u.load_class(None,'/Script/VirtualGameTeamProj.PowderBulkGameMode'))
assert levels.save_current_level()
(OUT/'display_classification.json').write_text(json.dumps(dict(parts=rows,frame_count=sum(r['frame'] for r in rows),
    cutaway_count=sum(r['cutaway'] for r in rows),mode='Full',frames=True,
    front='Initial +Unreal Y half-space; individual planes follow component transforms.',
    physics='Render-only: no collision, simulation, geometry or solver settings modified.'),ensure_ascii=False,indent=2),encoding='utf-8')
u.log('PLANT_DISPLAY_SETUP_COMPLETE')
