"""Duplicate the reference FlowTest into four independent, source-mesh sections."""
from pathlib import Path
import json,unreal as u
ROOT=Path('D:/GitHub/VirtualGameTeamProj');BASE='/Game/ProcessPlant_20261029'
specs=json.loads((ROOT/'SourceAssets/ProcessPlantSections/sections.json').read_text())
u.AssetRegistryHelpers.get_asset_registry().search_all_assets(synchronous_search=True)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
report=[]
for spec in specs:
 target=BASE+'/'+spec['level']
 assert not u.EditorAssetLibrary.does_asset_exist(target),'Refuse to overwrite '+target
 assert u.EditorAssetLibrary.duplicate_asset(BASE+'/L_ProcessPlant_FlowTest',target)
 assert levels.load_level(target)
 inventory=list(actors.get_all_level_actors());remove=[];display=None;test=None;camera=None
 for a in inventory:
  if a.get_class().get_name()=='PowderPlantTest':test=a
  if a.get_class().get_name()=='PlantVisibilityController':display=a
  if a.get_actor_label()=='Plant_OverviewCamera':camera=a
  if isinstance(a,u.StaticMeshActor) and not a.get_actor_label().startswith(tuple(spec['visual_prefixes'])):remove.append(a)
 assert test and display and camera
 # Remove stale references first; runtime render modes cannot resurrect removed parts.
 display.set_editor_property('parts',[p for p in display.get_editor_property('parts') if p.get_editor_property('actor') not in remove])
 for a in remove:assert actors.destroy_actor(a)
 test.set_actor_label('Plant_Section%02d_Test'%spec['section'])
 test.set_editor_property('section_test',spec['section']);test.set_editor_property('data_directory','SourceAssets/ProcessPlantSections/'+spec['name'])
 test.reset_simulation();assert test.get_editor_property('particle_count')==3000;assert not str(test.get_editor_property('error'))
 targetpos=u.Vector(*spec['camera_target_cm']);location=targetpos+u.Vector(150,450 if spec['section']<3 else 250,100)
 camera.set_actor_location(location,False,False);camera.set_actor_rotation(u.MathLibrary.find_look_at_rotation(location,targetpos),False)
 display.apply_display();assert levels.save_current_level()
 report.append(dict(level=target,section=spec['section'],removed_visual_actors=len(remove),remaining_actors=len(actors.get_all_level_actors()),particles=3000))
# The cancelled OBB experiment is no longer an active level. Keep source/report history.
obsolete=BASE+'/L_ProcessPlant_OBBTest'
if u.EditorAssetLibrary.does_asset_exist(obsolete):assert u.EditorAssetLibrary.delete_asset(obsolete)
file=(ROOT/'Content/ProcessPlant_20261029/L_ProcessPlant_OBBTest.umap').resolve();assert file.is_relative_to((ROOT/'Content').resolve())
if file.exists():
 try:file.unlink()
 except PermissionError:u.log_warning('Cancelled OBB map is open in another editor; leave its file intact.')
out=ROOT/'Docs/ProcessPlantPhysics/sections';out.mkdir(exist_ok=True)
(out/'levels.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
u.log('SECTION_LEVELS_READY '+json.dumps(report))
