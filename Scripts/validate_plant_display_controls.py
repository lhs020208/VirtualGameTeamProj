"""Exercise all six display combinations in the EDITOR world; never run PIE or physics."""
import json, hashlib
from pathlib import Path
import unreal as u
ROOT='/Game/ProcessPlant_20261029'
OUT=Path('D:/GitHub/VirtualGameTeamProj/Docs/ProcessPlant_20261029')
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
assert levels.load_level(ROOT+'/L_ProcessPlant_Assembly_Display')
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().startswith(ROOT+'/L_ProcessPlant_Assembly_Display.')
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
controller=next(a for a in actors.get_all_level_actors() if a.get_class().get_name()=='PlantVisibilityController')
parts=list(controller.get_editor_property('parts'))
assert len(parts)==1717
assert all(p.actor.get_world()==controller.get_world() for p in parts),'Cross-level part reference'
assert len({p.actor.get_path_name() for p in parts})==len(parts)

def component(p):return p.actor.get_component_by_class(u.StaticMeshComponent)
def physics_snapshot(p):
    c=component(p)
    t=c.get_world_transform()
    return dict(mesh=c.static_mesh.get_path_name(),transform=dict(location=list(t.translation.to_tuple()),
        scale=list(t.scale3d.to_tuple()),rotation=[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w]),
        collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name()),
        simulate=c.is_simulating_physics(),overlap=c.get_editor_property('generate_overlap_events'),
        materials=[c.get_material(i).get_path_name() for i in range(c.get_num_materials())],
        hidden_in_game=c.get_editor_property('hidden_in_game'))

# Collision-enabled fixture proves the invariant is not just testing pre-disabled plant collisions.
# No physics is enabled or stepped: BlockAll is only a static query/collision configuration here.
probe=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector(0,0,-10000))
probe.set_actor_label('Transient_DisplayCollisionProbe')
probe.static_mesh_component.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cube'))
probe.static_mesh_component.set_collision_profile_name('BlockAll')
probe_part=u.PlantVisualPart(actor=probe,cutaway=True)
sentinel=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector(0,0,-11000))
sentinel.set_actor_label('Transient_UnlistedParticleVisual')
sentinel.static_mesh_component.set_static_mesh(u.load_asset('/Engine/BasicShapes/Sphere'))
sentinel.static_mesh_component.set_visibility(True)
tested=parts+[probe_part]
controller.set_editor_property('parts',tested)
before=[physics_snapshot(p) for p in tested]
assert before[-1]['collision']!='<CollisionEnabled.NO_COLLISION: 0>'
checks=[]
try:
    for mode in [u.PlantViewMode.FULL,u.PlantViewMode.CUTAWAY,u.PlantViewMode.HIDDEN]:
        for frames in [True,False]:
            controller.set_view_mode(mode);controller.set_frames_visible(frames)
            visible=0
            for p in tested:
                c=component(p)
                expected=frames if p.frame else mode!=u.PlantViewMode.HIDDEN
                assert c.is_visible()==expected,p.actor.get_actor_label()
                visible+=int(c.is_visible())
                if p.cutaway:
                    data=list(c.get_editor_property('custom_primitive_data').get_editor_property('data'))
                    assert data[0]==(1. if mode==u.PlantViewMode.CUTAWAY else 0.)
                    assert len(data)>=5
            assert sentinel.static_mesh_component.is_visible(),'Unlisted particle visuals changed'
            after=[physics_snapshot(p) for p in tested]
            differences=[dict(name=p.actor.get_actor_label(),before=b,after=a) for p,b,a in zip(tested,before,after) if b!=a]
            assert not differences,repr(differences[:2])
            checks.append(dict(mode=str(mode),frames=frames,visible_including_fixture=visible,physics_unchanged=True,
                unlisted_visual_unchanged=True,cut_mask_correct=True))
finally:
    controller.set_editor_property('parts',parts)
    controller.show_full();controller.set_frames_visible(True)
    actors.destroy_actor(probe);actors.destroy_actor(sentinel)
assert levels.save_current_level()
(OUT/'display_validation.json').write_text(json.dumps(dict(passed=True,combinations=checks,parts=len(parts),
    frame_count=sum(p.frame for p in parts),cutaway_count=sum(p.cutaway for p in parts),
    collision_enabled_fixture=True,simulation_executed=False,
    note='State invariance and shader parameters only; no dynamic particle/contact test was run.'),ensure_ascii=False,indent=2),encoding='utf-8')
u.log('PLANT_DISPLAY_VALIDATION_PASSED')
