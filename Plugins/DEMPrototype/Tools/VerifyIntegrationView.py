import unreal, json
from pathlib import Path
world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/DEM/DEM_Integration')
assert world
actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
m=next(a for a in actors if 'BP_DEMWorld' in a.get_class().get_name())
e=next(a for a in actors if 'BP_DEMEmitter' in a.get_class().get_name())
c=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CameraActor)[0]
v=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PostProcessVolume)[0]
s=v.get_editor_property('settings')
r={'map':world.get_path_name(),'camera_player':c.get_auto_activate_player_index(),'speed':m.get_editor_property('speed_multiplier'),'substeps':m.get_editor_property('max_substeps'),'dt':m.get_editor_property('fixed_dt'),'count':e.get_editor_property('particle_count'),'exposure_min':s.get_editor_property('auto_exposure_min_brightness'),'exposure_max':s.get_editor_property('auto_exposure_max_brightness'),'physics_asset':e.get_editor_property('particle_settings').get_path_name()}
assert r['camera_player']==0 and r['speed']==.05 and r['substeps']==10000 and r['count']==200 and r['dt']==.00001
assert r['exposure_min']==r['exposure_max']==0
Path('C:/GitHub/VirtualGameTeamProj/Plugins/DEMPrototype/CommonValidation/view_reload.json').write_text(json.dumps(r,indent=2))
unreal.log('DEM_VIEW_RELOAD_PASS '+json.dumps(r))
