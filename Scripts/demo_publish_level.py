import unreal,pathlib,json,hashlib,shutil
root=pathlib.Path(unreal.Paths.project_dir()).resolve();out=root/'Docs/Demo200kSetup';out.mkdir(parents=True,exist_ok=True)
level='/Game/ProcessPlant_Demo20261119/L_ProcessPlant_Demo30FPS_1119'
path=root/'Content/ProcessPlant_Demo20261119/L_ProcessPlant_Demo30FPS_1119.umap'
backup=root/'Intermediate/Demo200kSetup/L_ProcessPlant_Demo30FPS_before_200k.umap'
backup.parent.mkdir(parents=True,exist_ok=True)
if not backup.exists():shutil.copy2(path,backup)
originals=json.loads((root/'Docs/Demo20261119/original_hashes.json').read_text())
assert unreal.EditorLoadingAndSavingUtils.load_map(level)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
tests=[x for x in actors if isinstance(x,unreal.PowderPlantTest)];assert len(tests)==1
a=tests[0];a.set_editor_property('demo_realtime',True);a.set_editor_property('demo_visual_copies',10);a.set_editor_property('demo_auto_cycle',True)
assert unreal.EditorLoadingAndSavingUtils.save_current_level()
assert a.get_editor_property('demo_visual_copies')==10
for p,h in originals.items():assert hashlib.sha256((root/p).read_bytes()).hexdigest()==h,p
(out/'level.json').write_text(json.dumps(dict(level=level,visual_copies=10,display_particles=200000,solver_particles=2500,originals_preserved=True,backup=str(backup)),indent=2))
unreal.log('DEMO_200K_LEVEL_SAVED')
