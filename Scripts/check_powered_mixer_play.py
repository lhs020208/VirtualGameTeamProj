"""PIE check through existing mixer GUI handlers, using a temporary 24-particle charge.

Restores the editor actor's source directory, never saves the temporary charge.
The standalone OpenCL test covers full run-up, loaded speed and stall duration.
"""
import unreal as u
import time, json, traceback, csv, shutil
from pathlib import Path

root = Path(u.Paths.project_dir()).resolve()
out = root / 'Docs/PoweredMixer1128/play_validation.json'
out.parent.mkdir(parents=True, exist_ok=True)
levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
assert not levels.is_in_play_in_editor()
source = next(a for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
              if a.get_class().get_name() == 'PowderPlantTest')
original = source.get_editor_property('data_directory')
assert original == 'SourceAssets/PlantRevision1125/Simulation'
assert source.get_editor_property('powered_mixer')
temporary = root / 'Intermediate/Mixer1128Data'
temporary.mkdir(parents=True, exist_ok=True)
for name in ['runtime.plant', 'runtime.routes']:
    shutil.copy2(root / original / name, temporary / name)
with (temporary / 'test_charge.csv').open('w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow(['id', 'species', 'supplier', 'x_m', 'y_m', 'z_m', 'radius_m', 'density_kg_m3'])
    for i in range(24):
        # A small lattice above the bowl floor, away from the stationary shaft.
        writer.writerow([i, i % 2, 'H-101A' if i % 2 == 0 else 'H-101B',
                         2.92 + .006 * (i % 6), -.015 + .006 * (i // 6), .62, .002, 1200])
source.set_editor_property('data_directory', 'Intermediate/Mixer1128Data')
started = time.monotonic()
phase = 0
handle = None
report = {'test_particles': 24}

def finish(error=None):
    if error:
        report['failure'] = error
    source.set_editor_property('data_directory', original)
    if handle:
        u.unregister_slate_post_tick_callback(handle)
    levels.editor_request_end_play()
    report['wall_seconds'] = time.monotonic() - started
    out.write_text(json.dumps(report, indent=2))
    print('POWERED_MIXER_TEST', report)

def tick(dt):
    global phase, test, ui, widget, rotors, initial, stop_time, stop_rpm
    try:
        if time.monotonic() - started > 240:
            raise RuntimeError('PIE timeout phase ' + str(phase))
        if phase == 0:
            worlds = u.EditorLevelLibrary.get_pie_worlds(False)
            if not worlds:
                return
            world = worlds[0]
            actors = u.GameplayStatics.get_all_actors_of_class(world, u.Actor)
            test = next(a for a in actors if a.get_class().get_name() == 'PowderPlantTest')
            assert not str(test.get_editor_property('error')), str(test.get_editor_property('error'))
            snap = test.get_simulation_snapshot()
            if not snap.mixer_drive.supported:
                return
            assert test.get_editor_property('particle_count') == 24
            assert snap.mixer_drive.stopped and len(snap.valve_actuators) == 8
            sub = next((o for o in u.ObjectIterator(u.ProcessUIStateSubsystem) if o.get_outer() == world), None)
            if not sub:
                return
            ui = next((o for o in u.ObjectIterator() if o.get_outer() == sub and o.get_class().get_name() == 'ProcessControlUI'), None)
            widget = sub.get_editor_property('status_widget')
            if not ui or not widget:
                return
            rotors = [a for a in actors if a.actor_has_tag('PlantMixerRotor')]
            assert len(rotors) == 47
            initial = [a.get_actor_transform() for a in rotors]
            widget.call_method('Show2D')
            widget.call_method('SelectEquipment_0', ('mixer', u.Text('혼합기'), u.Text('전력 기반 혼합기 시험'), True))
            ui.call_method('StopAll')
            phase = 1
        elif phase == 1:
            if test.get_simulation_snapshot().execution != u.PlantExecutionState.PAUSED:
                return
            ui.call_method('Resume')
            phase = 2
        elif phase == 2:
            if test.get_simulation_snapshot().execution != u.PlantExecutionState.RUNNING:
                return
            ui.call_method('StartMixer')
            phase = 3
        elif phase == 3:
            snap = test.get_simulation_snapshot()
            assert not str(test.get_editor_property('error')), str(test.get_editor_property('error'))
            motor = snap.mixer_drive
            assert not motor.tripped
            if motor.actual_rpm < .3:
                return
            assert motor.actual_rpm < 5 and motor.target_rpm > 14.9
            assert 0 < motor.electrical_watts <= 1302.01
            assert snap.mixer_angle_radians > 0
            # Renderer and physics use the same actual angle, world X only.
            import math
            for a, transform in zip(rotors, initial):
                q = transform.rotation
                delta = a.get_actor_transform().rotation * u.Quat(-q.x, -q.y, -q.z, q.w)
                assert abs(delta.y) < 1.e-4 and abs(delta.z) < 1.e-4
                # Render/readback can lag one worker batch; compare with 0.5 degree tolerance.
                assert abs(delta.rotator().roll + math.degrees(snap.mixer_angle_radians)) < .5
            report.update(actual_rpm=motor.actual_rpm, target_rpm=motor.target_rpm,
                          watts=motor.electrical_watts, torque_nm=motor.motor_torque_nm,
                          contact_nm=motor.contact_torque_nm, energy_j=motor.energy_joules,
                          angle_rad=snap.mixer_angle_radians, world_x_rotor_count=len(rotors),
                          other_valves_closed=all(v.closed for v in snap.valve_actuators))
            assert report['other_valves_closed']
            stop_time, stop_rpm = snap.physical_seconds, motor.actual_rpm
            ui.call_method('StopMixer')
            phase = 4
        elif phase == 4:
            snap = test.get_simulation_snapshot()
            assert not str(test.get_editor_property('error')), str(test.get_editor_property('error'))
            if not snap.mixer_drive.stopped or snap.worker_mixer_enabled:
                return
            assert snap.physical_seconds > stop_time
            report.update(stopped=True, stop_physical_seconds=snap.physical_seconds - stop_time,
                          final_rpm=snap.applied_rpm, final_energy_j=snap.mixer_drive.energy_joules,
                          physical_seconds=snap.physical_seconds, final_particles=test.get_editor_property('particle_count'))
            assert report['final_particles'] == 24 and report['final_rpm'] == 0
            finish()
    except Exception:
        finish(traceback.format_exc())

handle = u.register_slate_post_tick_callback(tick)
levels.editor_request_begin_play()
