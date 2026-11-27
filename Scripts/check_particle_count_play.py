"""GUI count/reset/recipe isolation and zero-particle real-time equipment check."""
import unreal as u
import time, json, traceback
from pathlib import Path
root = Path(u.Paths.project_dir()).resolve()
out = root/'Docs/ParticleCounts1128/play_validation.json'
out.parent.mkdir(parents=True, exist_ok=True)
levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
assert not levels.is_in_play_in_editor()
started = time.monotonic()
phase = 0
report = {}
handle = None
def finish(error=None):
    if error: report['failure'] = error
    if handle: u.unregister_slate_post_tick_callback(handle)
    levels.editor_request_end_play()
    report['wall_seconds'] = time.monotonic()-started
    out.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')
    print('PARTICLE_COUNT_TEST', report)
def select(name):
    widget.call_method('SelectEquipment_0', (name, u.Text(name), u.Text(''), True))
def apply(counts):
    for field, value in zip(fields, counts): field.set_text(u.Text(str(value)))
    ui.call_method('ApplyParticleSettings')
def recipe():
    r = service.get_applied_recipe()
    return [r.export_text(), service.get_editor_property('recipe_revision')]
def tick(dt):
    global phase, test, ui, widget, fields, service, original, saved_recipe, epoch, mark, start_s, pause_s
    try:
        now = time.monotonic()
        if now-started > 180: raise RuntimeError('timeout phase '+str(phase))
        if phase == 0:
            worlds = u.EditorLevelLibrary.get_pie_worlds(False)
            if not worlds: return
            world = worlds[0]
            test = next(a for a in u.GameplayStatics.get_all_actors_of_class(world, u.PowderPlantTest))
            snap = test.get_simulation_snapshot()
            if not snap.mixer_drive.supported: return
            sub = next(o for o in u.ObjectIterator(u.ProcessUIStateSubsystem) if o.get_outer()==world)
            ui = next((o for o in u.ObjectIterator() if o.get_outer()==sub and o.get_class().get_name()=='ProcessControlUI'), None)
            if not ui:return
            service = next(o for o in u.ObjectIterator(u.ProcessControlService) if o.get_outer()==sub)
            widget = sub.get_editor_property('status_widget')
            widget.call_method('Show2D')
            ui.call_method('ToggleParticleSettings')
            fields = sorted([o for o in u.ObjectIterator(u.EditableTextBox) if o.get_name() in ['ParticleCount'+str(i) for i in range(6)] and widget.get_path_name() in o.get_path_name()], key=lambda o:o.get_name())
            assert len(fields)==6, [f.get_path_name() for f in fields]
            original = [100,100,0,0,0,0]
            saved_recipe = recipe()
            epoch = snap.epoch
            apply([-1,3,0,0,0,0])
            assert test.get_simulation_snapshot().epoch==epoch, 'invalid input reset simulation'
            apply([2,3,1,0,0,0])
            phase = 1
        elif phase == 1:
            assert not str(test.get_editor_property('error')), str(test.get_editor_property('error'))
            snap = test.get_simulation_snapshot()
            if not snap.mixer_drive.supported: return
            assert test.get_editor_property('particle_count')==6
            assert list(test.get_initial_particle_counts())==[2,3,1,0,0,0]
            assert recipe()==saved_recipe, 'recipe changed by particle count'
            report['positive_count'] = 6
            ui.call_method('ClearParticleDraft')
            ui.call_method('ApplyParticleSettings')
            phase = 2
        elif phase == 2:
            assert not str(test.get_editor_property('error')), str(test.get_editor_property('error'))
            snap = test.get_simulation_snapshot()
            if not snap.mixer_drive.supported: return
            assert snap.equipment_only and snap.total_particle_count==0
            assert 'CPU equipment only' in snap.gpu_device
            assert recipe()==saved_recipe
            report['empty_device'] = snap.gpu_device
            ui.call_method('ToggleParticleSettings')
            ui.call_method('StopAll')
            phase = 3
        elif phase == 3:
            snap = test.get_simulation_snapshot()
            if snap.execution!=u.PlantExecutionState.PAUSED: return
            ui.call_method('Resume')
            phase = 4
        elif phase == 4:
            snap = test.get_simulation_snapshot()
            if snap.execution!=u.PlantExecutionState.RUNNING: return
            select('mixer');ui.call_method('StartMixer')
            select('supply-a');ui.call_method('OpenValve')
            mark, start_s = now, snap.physical_seconds
            phase = 5
        elif phase == 5:
            snap = test.get_simulation_snapshot()
            assert not str(test.get_editor_property('error')), str(test.get_editor_property('error'))
            if now-mark<13: return
            rate = (snap.physical_seconds-start_s)/(now-mark)
            report.update(real_time_rate=rate, measured_wall_seconds=now-mark, measured_sim_seconds=snap.physical_seconds-start_s,
                          rpm=snap.mixer_drive.actual_rpm, valve_a_degrees=snap.valve_actuators[0].angle_degrees)
            assert .95<rate<1.05, rate
            assert 14.8<snap.mixer_drive.actual_rpm<15.1, snap.mixer_drive
            assert snap.valve_actuators[0].open, snap.valve_actuators[0]
            ui.call_method('StopAll')
            phase = 6
        elif phase == 6:
            snap = test.get_simulation_snapshot()
            if snap.execution!=u.PlantExecutionState.PAUSED: return
            assert snap.mixer_drive.stopped and all(v.closed for v in snap.valve_actuators)
            pause_s, mark = snap.physical_seconds, now
            phase = 7
        elif phase == 7:
            if now-mark<1: return
            assert test.get_simulation_snapshot().physical_seconds==pause_s, 'paused clock advanced'
            ui.call_method('Resume')
            phase = 8
        elif phase == 8:
            snap = test.get_simulation_snapshot()
            if snap.execution!=u.PlantExecutionState.RUNNING: return
            select('weigher');ui.call_method('OpenValve');mark=now
            phase = 9
        elif phase == 9:
            snap = test.get_simulation_snapshot()
            if now-mark<13: return
            assert snap.valve_actuators[6].open, snap.valve_actuators[6]
            report['empty_weigher_open'] = True
            ui.call_method('ToggleParticleSettings');apply([2,3,1,0,0,0]);phase=10
        elif phase == 10:
            snap = test.get_simulation_snapshot()
            assert not str(test.get_editor_property('error')), str(test.get_editor_property('error'))
            if not snap.mixer_drive.supported or snap.physical_seconds<=0: return
            assert test.get_editor_property('particle_count')==6 and not snap.equipment_only
            assert 'CPU equipment only' not in snap.gpu_device
            assert recipe()==saved_recipe
            report['return_to_gpu'] = snap.gpu_device
            apply(original);phase=11
        elif phase == 11:
            snap = test.get_simulation_snapshot()
            assert not str(test.get_editor_property('error')), str(test.get_editor_property('error'))
            if not snap.mixer_drive.supported: return
            assert list(test.get_initial_particle_counts())==original and recipe()==saved_recipe
            report.update(restored_counts=original, recipe_unchanged=True, pause_holds_time=True)
            assert json.loads((root/'Saved/ProcessUI/ParticleCounts.json').read_text())['counts']==original
            apply([0]*6)
            levels.editor_request_end_play(); mark=now; phase=12
        elif phase == 12:
            if levels.is_in_play_in_editor() or now-mark<1: return
            levels.editor_request_begin_play();phase=13
        elif phase == 13:
            worlds=u.EditorLevelLibrary.get_pie_worlds(False)
            if not worlds:return
            world=worlds[0]
            test=next(iter(u.GameplayStatics.get_all_actors_of_class(world,u.PowderPlantTest)))
            snap=test.get_simulation_snapshot()
            assert not str(test.get_editor_property('error')), str(test.get_editor_property('error'))
            if not snap.mixer_drive.supported:return
            assert snap.equipment_only and 'CPU equipment only' in snap.gpu_device
            report['zero_mode_restored_on_new_play']=True
            sub=next(o for o in u.ObjectIterator(u.ProcessUIStateSubsystem) if o.get_outer()==world)
            ui=next((o for o in u.ObjectIterator() if o.get_outer()==sub and o.get_class().get_name()=='ProcessControlUI'),None)
            if not ui:return
            service=next(o for o in u.ObjectIterator(u.ProcessControlService) if o.get_outer()==sub)
            widget=sub.get_editor_property('status_widget')
            widget.call_method('Show2D');ui.call_method('ToggleParticleSettings')
            fields=sorted([o for o in u.ObjectIterator(u.EditableTextBox) if o.get_name() in ['ParticleCount'+str(i) for i in range(6)] and widget.get_path_name() in o.get_path_name()],key=lambda o:o.get_name())
            assert len(fields)==6
            apply(original);phase=14
        elif phase == 14:
            snap=test.get_simulation_snapshot()
            if not snap.mixer_drive.supported:return
            assert test.get_editor_property('particle_count')==sum(original)
            assert json.loads((root/'Saved/ProcessUI/ParticleCounts.json').read_text())['counts']==original
            finish()
    except Exception: finish(traceback.format_exc())
handle = u.register_slate_post_tick_callback(tick)
levels.editor_request_begin_play()
