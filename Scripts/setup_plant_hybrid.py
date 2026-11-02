"""Create one extra full-process hybrid level; keep all existing levels."""
from pathlib import Path
import json,unreal as u
root=Path('D:/GitHub/VirtualGameTeamProj');base='/Game/ProcessPlant_20261029';target=base+'/L_ProcessPlant_HybridCollisionTest'
u.AssetRegistryHelpers.get_asset_registry().search_all_assets(synchronous_search=True)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
assert not u.EditorAssetLibrary.does_asset_exist(target),'Refuse to overwrite an existing hybrid test'
assert u.EditorAssetLibrary.duplicate_asset(base+'/L_ProcessPlant_FlowTest',target);assert levels.load_level(target)
found=[a for a in actors.get_all_level_actors() if a.get_class().get_name()=='PowderPlantTest'];assert len(found)==1
a=found[0];a.set_editor_property('data_directory','SourceAssets/ProcessPlantHybrid');a.set_editor_property('section_test',0);a.set_actor_label('Plant_Hybrid_Pressure_Grains_Test');a.reset_simulation();assert a.get_editor_property('particle_count')==30;assert not str(a.get_editor_property('error'))
assert levels.save_current_level()
(root/'Docs/ProcessPlantPhysics/hybrid/level.json').write_text(json.dumps(dict(level=target,particles=30,actor=a.get_actor_label(),data_directory=a.get_editor_property('data_directory')),indent=2))
u.log('HYBRID_LEVEL_READY '+target)
