"""Short zero-particle GUI-path check; ends PIE and leaves the saved map untouched."""
import unreal as u
import json, math, time, traceback
from pathlib import Path

root = Path(u.Paths.project_dir()).resolve()
out = root / 'Docs/MixerObstacle1128/play_validation.json'
out.parent.mkdir(parents=True, exist_ok=True)
levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
assert not levels.is_in_play_in_editor()
profile = (root / 'Saved/ProcessUI/ParticleCounts.json').read_bytes()
assert sum(json.loads(profile)['counts']) == 0, 'This test requires the existing zero-particle profile'
source = next(a for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
              if a.actor_has_tag('MixerFixedObstacle'))
original = source.get_actor_transform().export_text()
report = {'peak_contact_nm': 0, 'peak_motor_nm': 0, 'blocked_samples': 0}
phase, handle = 0, None
started = time.perf_counter()

def finish(error=None):
    if error:
        report['failure'] = error
    if handle:
        u.unregister_slate_post_tick_callback(handle)
    levels.editor_request_end_play()
    report['wall_seconds'] = time.perf_counter() - started
    report['particle_profile_unchanged'] = profile == (root / 'Saved/ProcessUI/ParticleCounts.json').read_bytes()
    report['editor_block_unchanged'] = source.get_actor_transform().export_text() == original
    out.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print('FIXED_MIXER_OBSTACLE_CHECK', report)

def tick(dt):
    global phase, test, ui, widget, block, initial, rotors
    try:
        assert time.perf_counter() - started < 35, 'obstacle PIE check timed out'
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
            assert snap.equipment_only and snap.total_particle_count == 0
            sub = next((o for o in u.ObjectIterator(u.ProcessUIStateSubsystem) if o.get_outer() == world), None)
            if not sub:
                return
            ui = next((o for o in u.ObjectIterator() if o.get_outer() == sub and o.get_class().get_name() == 'ProcessControlUI'), None)
            widget = sub.get_editor_property('status_widget')
            if not ui or not widget:
                return
            block = next(a for a in actors if a.actor_has_tag('MixerFixedObstacle'))
            initial = block.get_actor_transform().export_text()
            rotors = [(a, a.get_actor_transform()) for a in actors if a.actor_has_tag('PlantMixerRotor')]
            widget.call_method('Show2D')
            widget.call_method('SelectEquipment_0', ('mixer', u.Text('혼합기'), u.Text('고정 장애물 접촉 시험'), True))
            ui.call_method('StopAll')
            phase = 1
        elif phase == 1:
            if test.get_simulation_snapshot().execution == u.PlantExecutionState.PAUSED:
                ui.call_method('Resume')
                phase = 2
        elif phase == 2:
            if test.get_simulation_snapshot().execution == u.PlantExecutionState.RUNNING:
                ui.call_method('StartMixer')
                phase = 3
        else:
            assert not str(test.get_editor_property('error')), str(test.get_editor_property('error'))
            snap = test.get_simulation_snapshot()
            motor = snap.mixer_drive
            assert snap.total_particle_count == 0
            assert block.get_actor_transform().export_text() == initial
            assert snap.mixer_angle_radians <= .501517, 'visible shaft passed actual geometric contact'
            report['peak_contact_nm'] = max(report['peak_contact_nm'], abs(motor.contact_torque_nm))
            report['peak_motor_nm'] = max(report['peak_motor_nm'], abs(motor.motor_torque_nm))
            if motor.stopped and abs(motor.contact_torque_nm) > 700:
                report['blocked_samples'] += 1
                report.setdefault('first_blocked_seconds', snap.physical_seconds)
                report['last_blocked_seconds'] = snap.physical_seconds
            if not motor.tripped:
                return
            # Background editor Slate callbacks can be throttled; verify elapsed
            # physical hold time instead of assuming a foreground callback rate.
            assert motor.stopped and report['blocked_samples'] >= 2
            assert snap.physical_seconds - report['first_blocked_seconds'] >= 1.5
            assert report['peak_contact_nm'] > 700 and report['peak_motor_nm'] <= 750.01
            # Compare displayed mesh orientation with the constrained physical angle.
            report['rotor_pose_errors'] = []
            for actor, transform in rotors:
                q = transform.rotation
                delta = actor.get_actor_transform().rotation * u.Quat(-q.x, -q.y, -q.z, q.w)
                assert abs(delta.y) < 1.e-4 and abs(delta.z) < 1.e-4
                # FRotator.Roll has the opposite sign to axis-angle X in UE.
                half = snap.mixer_angle_radians / 2
                alignment = abs(-delta.x * math.sin(half) + delta.w * math.cos(half))
                error = 2 * math.acos(min(1, alignment))
                report['rotor_pose_errors'].append(error)
                assert error < 1.e-5, (actor.get_actor_label(), error, delta.export_text())
            report.update(angle_rad=snap.mixer_angle_radians, angle_degrees=math.degrees(snap.mixer_angle_radians),
                          actual_rpm=motor.actual_rpm, tripped=motor.tripped,
                          rotor_count=len(rotors), gpu_device=snap.gpu_device,
                          max_fps=u.SystemLibrary.get_console_variable_float_value('t.MaxFPS'))
            finish()
    except Exception:
        finish(traceback.format_exc())

handle = u.register_slate_post_tick_callback(tick)
levels.editor_request_begin_play()
