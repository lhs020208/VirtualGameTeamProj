"""Record a bounded, real PIE run. Each callback owns its handle and state."""
import unreal, json, time
from pathlib import Path
class DEMGUIRecorder:
    def __init__(self):
        self.world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        assert self.world, 'PIE required'
        actors=unreal.GameplayStatics.get_all_actors_of_class(self.world,unreal.Actor)
        self.manager=next(a for a in actors if 'BP_DEMWorld' in a.get_class().get_name())
        e=next(a for a in actors if 'BP_DEMEmitter' in a.get_class().get_name())
        self.ism=e.get_components_by_class(unreal.InstancedStaticMeshComponent)[0]
        self.rows=[];self.done=False;self.start=time.monotonic();self.last=self.start
        self.out=Path('C:/GitHub/VirtualGameTeamProj/Plugins/DEMPrototype/CommonValidation/gui_pie_trace_final.json')
        self.manager.reset();self.manager.start()
        self.rows.append(self.sample())
        self.handle=unreal.register_slate_post_tick_callback(self.tick)
        unreal.log('DEM_FINAL_GUI_TRACE_STARTED')
    def sample(self):
        keys=['status','simulation_time','particle_count','lag','running','reset_required','kinetic_energy','maximum_overlap_ratio','contact_count','non_finite_or_failed']
        r={k:self.manager.get_editor_property(k) for k in keys}
        zz=[self.ism.get_instance_transform(i,world_space=True).translation.z for i in range(self.ism.get_instance_count())]
        r.update(instance_count=len(zz),min_z_cm=min(zz) if zz else None,max_z_cm=max(zz) if zz else None,wall_seconds=time.monotonic()-self.start)
        return r
    def tick(self,delta):
        if self.done or time.monotonic()-self.last<.5:return
        self.last=time.monotonic()
        try:
            r=self.sample();self.rows.append(r)
            self.out.write_text(json.dumps(self.rows,indent=2))
            if r['simulation_time']>=5 or not r['running'] or r['wall_seconds']>200:
                self.done=True;unreal.unregister_slate_post_tick_callback(self.handle)
                self.manager.pause()
                unreal.log('DEM_FINAL_GUI_TRACE_DONE '+json.dumps(r))
        except Exception:
            self.done=True;unreal.unregister_slate_post_tick_callback(self.handle)
            raise
DEMGUIRecorder()
