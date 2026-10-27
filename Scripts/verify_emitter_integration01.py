import unreal as u
import json, traceback
from pathlib import Path
report = {"success": False}
actor = None
try:
    settings = u.load_asset('/Game/DEM/Settings/DA_DEMParticle_Default')
    mesh = u.load_asset('/Game/DEM/SM_DEMSphere')
    bp = u.load_asset('/Game/DEM/Emitters/BP_DEMEmitter')
    assert settings and mesh and bp, 'Imported assets must load'
    cls = u.load_class(None, '/Script/DEMPrototype.DEMEmitter')
    bpclass = u.load_class(None, '/Game/DEM/Emitters/BP_DEMEmitter.BP_DEMEmitter_C')
    assert cls and bpclass
    u.EditorLoadingAndSavingUtils.new_blank_map(False)
    actor = u.get_editor_subsystem(u.EditorActorSubsystem).spawn_actor_from_class(bpclass, u.Vector(100,200,300))
    assert actor
    actor.set_editor_property('particle_settings', settings)
    actor.set_editor_property('particle_count', 3000)
    actor.refresh_preview()
    assert actor.get_editor_property('preview_only')
    assert actor.get_editor_property('visible_instance_count') == 2048
    assert actor.get_editor_property('generated_count') == 0
    assert actor.try_generate()
    assert actor.get_editor_property('generated_count') == 3000
    assert actor.get_editor_property('visible_instance_count') == 3000
    actor.clear()
    assert actor.get_editor_property('visible_instance_count') == 0
    actor.set_actor_scale3d(u.Vector(2,1,1))
    assert not actor.try_generate(), 'Non-unit scale must be rejected'
    actor.set_actor_scale3d(u.Vector(1,1,1))
    actor.set_editor_property('particle_count', 50000)
    assert actor.try_generate()
    assert actor.get_editor_property('generated_count') == 50000
    actor.clear()
    report.update(success=True, checks=['asset/class load','preview cap 2048 without exported particles','3000 full burst','clear','non-unit scale rejected','50000 full burst'])
except Exception:
    report['error'] = traceback.format_exc()
    u.log_error(report['error'])
finally:
    if actor: u.get_editor_subsystem(u.EditorActorSubsystem).destroy_actor(actor)
    Path('D:/GitHub/VirtualGameTeamProj/Docs/DEM/emitter_integration01/verification.json').write_text(json.dumps(report, indent=2))

