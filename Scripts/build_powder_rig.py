"""Rebuild only tagged test fixtures in L_Main. Run with Unreal Python commandlet."""
import math
import unreal as u

levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
assert levels.load_level('/Game/L_Main')
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
for a in actors.get_all_level_actors():
    if a.actor_has_tag('PowderRig'):
        actors.destroy_actor(a)
cube=u.load_asset('/Engine/BasicShapes/Cube.Cube')
assert cube
made=[]
def box(name,xyz,size,folder,rotation=(0,0,0),tube=False,cut=False):
    a=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector(*xyz),u.Rotator(pitch=rotation[0],yaw=rotation[1],roll=rotation[2]))
    a.set_actor_label(name)
    a.set_folder_path('PowderTest/'+folder)
    a.tags=[u.Name('PowderRig')]+([u.Name('PowderTube')] if tube else [])+([u.Name('PowderCutaway')] if cut else [])
    for prefix,tag in (('Feed_Wall_','PowderFeed'),('Feed_Funnel_','PowderFunnel'),('Collector_','PowderCollector')):
        if name.startswith(prefix): a.tags=list(a.tags)+[u.Name(tag)]
    m=a.static_mesh_component
    m.set_static_mesh(cube)
    m.set_collision_profile_name('BlockAll')
    a.set_actor_scale3d(u.Vector(*(x/100 for x in size)))
    made.append(a)
    return a

for i in range(16):
    angle=2*math.pi*i/16
    nx,ny=math.cos(angle),math.sin(angle)
    yaw=math.degrees(angle)
    cut=nx*130-ny*180>0
    # All front walls remain collidable; only Play rendering is cut away.
    box(f'Feed_Wall_{i:02}',(32*nx,32*ny,510),(4,14,140),'01_Feed',(0,yaw,0),cut=cut)
    pitch=-math.degrees(math.atan2(18,80))
    box(f'Feed_Funnel_{i:02}',(22.95*nx,22.95*ny,399.56),(4,14,math.hypot(18,80)+2),'02_Funnel',(pitch,yaw,0),cut=cut)
    box(f'Tube_Wall_{i:02}',(14*nx,14*ny,300),(4,6.7,120),'03_Tube',(0,yaw,0),tube=True,cut=cut)
box('Collector_Base',(0,0,100),(160,160,8),'04_Collector')
box('Collector_Left',(-78,0,157),(4,160,106),'04_Collector')
box('Collector_Right',(78,0,157),(4,160,106),'04_Collector')
box('Collector_Back',(0,78,157),(160,4,106),'04_Collector')
box('Collector_Front',(0,-78,157),(160,4,106),'04_Collector',cut=True)
assert len(made)==53
assert sum(a.actor_has_tag('PowderTube') for a in made)==16
assert levels.save_current_level()
u.log('POWDER_RIG_SAVED: 53 fixture actors; feed 440..580; funnel 360..440; tube 240..360; collector 104..210 cm')
