"""Create OBB experiment and retire only unreferenced legacy World assets."""
from pathlib import Path
import json,unreal as u
ROOT=Path('D:/GitHub/VirtualGameTeamProj')
base='/Game/ProcessPlant_20261029'; target=base+'/L_ProcessPlant_OBBTest'
registry=u.AssetRegistryHelpers.get_asset_registry();registry.search_all_assets(synchronous_search=True)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
assert not u.EditorAssetLibrary.does_asset_exist(target), 'Refuse to overwrite existing OBB level'
assert u.EditorAssetLibrary.duplicate_asset(base+'/L_ProcessPlant_FlowTest',target)
assert levels.load_level(target)
found=[]
for actor in actors.get_all_level_actors():
    if actor.get_class().get_name()=='PowderPlantTest':
        actor.set_editor_property('data_directory','SourceAssets/ProcessPlantOBB')
        actor.set_actor_label('Plant_OBB_Pressure_Grains_Test')
        actor.reset_simulation();assert actor.get_editor_property('particle_count')==3000
        assert not str(actor.get_editor_property('error'));found.append(actor.get_actor_label())
assert len(found)==1
assert levels.save_current_level()
keep={target,base+'/L_ProcessPlant_FlowTest',base+'/L_ProcessPlant_Assembly',base+'/L_ProcessPlant_Assembly_Display'}
worlds=[str(a.package_name) for a in registry.get_assets_by_class(u.TopLevelAssetPath('/Script/Engine','World'),True) if str(a.package_name).startswith('/Game/')]
candidates=set(worlds)-keep
report=dict(level=target,actor=found,kept=sorted(keep),deleted=[],retained_references={})
# Check the complete set before deleting. A dependency of a retained asset must stay.
changed=True
while changed:
    changed=False
    for path in list(candidates):
        refs=set(u.EditorAssetLibrary.find_package_referencers_for_asset(path,False))-{path}
        external=refs-candidates
        if external:
            report['retained_references'][path]=sorted(external);candidates.remove(path);changed=True
for path in sorted(candidates):
    assert u.EditorAssetLibrary.delete_asset(path),path
    file=(ROOT/'Content'/(path.removeprefix('/Game/')+'.umap')).resolve()
    assert file.is_relative_to((ROOT/'Content').resolve())
    if file.exists(): file.unlink() # Some commandlet builds remove the registry asset only.
    assert not file.exists()
    report['deleted'].append(path)
(ROOT/'Docs/ProcessPlantPhysics/obb/levels.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
u.log('OBB_LEVEL_READY '+json.dumps(report,ensure_ascii=False))
