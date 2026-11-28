import unreal,pathlib,json,hashlib
root=pathlib.Path(unreal.Paths.project_dir()).resolve()
source='/Game/ProcessPlant_20261111_LowPoly/L_ProcessPlant_LowPoly_1111'
dest='/Game/ProcessPlant_Demo20261119/L_ProcessPlant_Demo30FPS_1119'
receipt=json.loads((root/'Docs/Demo20261119/original_hashes.json').read_text())
if not unreal.EditorAssetLibrary.does_asset_exist(dest):
    assert unreal.EditorAssetLibrary.duplicate_asset(source,dest)
assert unreal.EditorLoadingAndSavingUtils.load_map(dest)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
solvers=[a for a in actors if isinstance(a,unreal.PowderPlantTest)]
assert len(solvers)==1
a=solvers[0];a.set_editor_property('demo_realtime',True);a.set_editor_property('demo_auto_cycle',True)
a.set_editor_property('compute_backend',unreal.PlantComputeBackend.OPEN_CL)
a.set_editor_property('running',True)
for actor in actors:
    if isinstance(actor,unreal.PlantVisibilityController): actor.set_editor_property('view_mode',unreal.PlantViewMode.CUTAWAY)
assert unreal.EditorLoadingAndSavingUtils.save_current_level()
for p,h in receipt.items(): assert hashlib.sha256((root/p).read_bytes()).hexdigest()==h,p
(root/'Docs/Demo20261119/level.json').write_text(json.dumps(dict(source=source,level=dest,demo=True,auto_cycle=True,originals_preserved=True),indent=2))
unreal.log('DEMO_LEVEL_CREATED_AND_VERIFIED')
