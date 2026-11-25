"""Import the corrected scene once; setup_plant_revision1125 replaces old visuals.

Open L_ProcessPlant_LowPoly_1111 and run this in the Unreal Python console.
Wait for import completion before running setup_plant_revision1125.py.
"""
import unreal as u,json,shutil
from pathlib import Path
p=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir()))
(p/'Intermediate/Plant1125').mkdir(parents=True,exist_ok=True)
es=u.get_editor_subsystem(u.EditorActorSubsystem);ls=u.get_editor_subsystem(u.LevelEditorSubsystem)
w=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
assert w.get_path_name().endswith('L_ProcessPlant_LowPoly_1111.L_ProcessPlant_LowPoly_1111')
assert ls.save_current_level()
shutil.copy2(p/'Content/ProcessPlant_20261111_LowPoly/L_ProcessPlant_LowPoly_1111.umap',p/'Intermediate/Plant1125/BeforeRevision.umap')
old=es.get_all_level_actors()
(p/'Intermediate/Plant1125/old_actors.json').write_text(json.dumps([a.get_path_name() for a in old]))
options=u.ImportAssetParameters();options.is_automated=True
options.override_pipelines=[u.SoftObjectPath('/Interchange/Pipelines/DefaultSceneAssetsPipeline.DefaultSceneAssetsPipeline'),u.SoftObjectPath('/Interchange/Pipelines/DefaultSceneLevelPipeline.DefaultSceneLevelPipeline')]
m=u.InterchangeManager.get_interchange_manager_scripted()
assert m.import_scene('/Game/ProcessPlant_20261111_LowPoly/Revision1125/Assets',m.create_source_data(str(p/'SourceAssets/PlantRevision1125/Plant.fbx')),options)
print('REVISION_IMPORT_ACCEPTED')
