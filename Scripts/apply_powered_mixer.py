"""Enable the load-coupled OpenCL gearmotor on the current revision-1125 level."""
import unreal as u
import json
from pathlib import Path

levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
assert not levels.is_in_play_in_editor()
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().startswith('/Game/ProcessPlant_20261111_LowPoly/L_ProcessPlant_LowPoly_1111.')
actor = next(a for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
             if a.get_class().get_name() == 'PowderPlantTest')
assert actor.get_editor_property('data_directory') == 'SourceAssets/PlantRevision1125/Simulation'
assert actor.get_editor_property('compute_backend') == u.PlantComputeBackend.OPEN_CL
settings = dict(powered_mixer=True, mixer_input_power_w=1302.0, mixer_max_torque_nm=750.0,
                mixer_inertia_kg_m2=60.0, mixer_drive_efficiency=0.8, mixer_power_fraction=1.0,
                mixer_ramp_seconds=2.0, mixer_brake_torque_nm=100.0, mixer_trip_seconds=2.0)
for name, value in settings.items():
    actor.set_editor_property(name, value)
actor.call_method('ResetSimulation')
assert not str(actor.get_editor_property('error'))
assert levels.save_current_level()
out = Path(u.Paths.project_dir()) / 'Docs/PoweredMixer1128/level_settings.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(dict(level=world.get_path_name(), actor=actor.get_actor_label(),
                              particle_count=actor.get_editor_property('particle_count'),
                              **settings), indent=2))
print('POWERED_MIXER_APPLIED', settings)
