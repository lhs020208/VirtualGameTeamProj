import unreal as u
import math,json,traceback
from pathlib import Path
report={'success':False}; actors=[]
try:
 u.EditorLoadingAndSavingUtils.new_blank_map(False)
 subsystem=u.get_editor_subsystem(u.EditorActorSubsystem)
 mixer=subsystem.spawn_actor_from_class(u.load_class(None,'/Script/VirtualGameTeamProj.PowderGranularMixer'),u.Vector())
 actors.append(mixer)
 mixer.set_editor_property('particle_count',50000)
 assert mixer.validate_simulation_settings()
 assert abs(mixer.get_editor_property('charge_mass_kg')-64.8)<0.0001
 settings=u.new_object(u.load_class(None,'/Script/DEMPrototype.DEMParticleSettings'))
 mixer.set_editor_property('particle_settings',settings)
 assert not mixer.validate_simulation_settings(), 'CPU settings must not silently apply'
 settings.set_editor_property('gpu_approximation',True)
 settings.set_editor_property('radius_meters',.006)
 settings.set_editor_property('density_kg_per_cubic_meter',1500)
 assert mixer.validate_simulation_settings()
 mass=mixer.get_editor_property('charge_mass_kg')
 assert abs(mass-50000*1500*4*math.pi/3*.006**3)<0.0001
 settings.set_editor_property('wall_friction',.3)
 assert not mixer.validate_simulation_settings()
 settings.set_editor_property('wall_friction',.4)
 settings.set_editor_property('radius_meters',.001)
 assert not mixer.validate_simulation_settings()
 settings.set_editor_property('radius_meters',.006)
 settings.set_editor_property('density_kg_per_cubic_meter',float('nan'))
 assert not mixer.validate_simulation_settings()
 settings.set_editor_property('density_kg_per_cubic_meter',1500)
 mixer.set_editor_property('particle_count',0)
 assert not mixer.validate_simulation_settings()
 mixer.set_editor_property('particle_count',50000)
 assert mixer.validate_simulation_settings()
 emitter=subsystem.spawn_actor_from_class(u.load_class(None,'/Script/DEMPrototype.DEMEmitter'),u.Vector())
 actors.append(emitter)
 settings.set_editor_property('sphere_mesh',u.load_asset('/Game/DEM/SM_DEMSphere'))
 emitter.set_editor_property('particle_settings',settings)
 emitter.set_editor_property('initial_angular_velocity',u.Vector(1,0,0))
 assert not emitter.try_generate()
 emitter.set_editor_property('initial_angular_velocity',u.Vector())
 assert emitter.try_generate()
 gpu_path='/Game/DEM/Settings/DA_DEMParticle_GPU_Default'
 gpu=u.load_asset(gpu_path) if u.EditorAssetLibrary.does_asset_exist(gpu_path) else u.EditorAssetLibrary.duplicate_asset('/Game/DEM/Settings/DA_DEMParticle_Default',gpu_path)
 assert gpu
 gpu.set_editor_property('gpu_approximation',True)
 gpu.set_editor_property('radius_meters',(.054/(50000*4*math.pi/3))**(1/3))
 gpu.set_editor_property('density_kg_per_cubic_meter',1200)
 gpu.set_editor_property('particle_friction',.4)
 gpu.set_editor_property('wall_friction',.4)
 assert u.EditorAssetLibrary.save_loaded_asset(gpu)
 mixer.set_editor_property('particle_settings',gpu)
 assert mixer.validate_simulation_settings()
 assert abs(mixer.get_editor_property('charge_mass_kg')-64.8)<.0001
 report.update(success=True,custom_mass_kg=mass,checks=['legacy 64.8kg','CPU asset rejected','GPU custom mass','unequal friction rejected','radius limit','NaN density','zero count','GPU spin rejected','GPU static generation'])
except Exception:
 report['error']=traceback.format_exc();u.log_error(report['error'])
finally:
 for a in actors:u.get_editor_subsystem(u.EditorActorSubsystem).destroy_actor(a)
 Path('D:/GitHub/VirtualGameTeamProj/Docs/DEM/emitter_integration02/verification.json').write_text(json.dumps(report,indent=2))
