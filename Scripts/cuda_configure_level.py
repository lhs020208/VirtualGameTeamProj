import hashlib
import json
import pathlib
import unreal

root=pathlib.Path(unreal.Paths.project_dir()).resolve()
assert root.name=='VirtualGameTeamProj_CUDA_20261118'
original=root/'Content/ProcessPlant_20261111_LowPoly/L_ProcessPlant_LowPoly_1111.umap'
before=hashlib.sha256(original.read_bytes()).hexdigest()
path='/Game/ProcessPlant_CUDA_20261118/L_ProcessPlant_CUDA_1118'
assert unreal.EditorLoadingAndSavingUtils.load_map(path)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
solvers=[a for a in actors if isinstance(a,unreal.PowderPlantTest)]
assert len(solvers)==1, len(solvers)
actor=solvers[0]
assert actor.get_editor_property('data_directory')=='SourceAssets/ProcessPlant_20261111_LowPoly/Simulation'
assert abs(actor.get_editor_property('fixed_dt')-1e-5)<1e-12
assert actor.get_editor_property('contact_iterations')==8
actor.set_editor_property('compute_backend',unreal.PlantComputeBackend.CUDA_PTX_GRAPH)
assert unreal.EditorLoadingAndSavingUtils.save_current_level()
assert hashlib.sha256(original.read_bytes()).hexdigest()==before
report=dict(level=path, actor=actor.get_actor_label(),
            backend=str(actor.get_editor_property('compute_backend')),
            data_directory=actor.get_editor_property('data_directory'),
            fixed_dt=actor.get_editor_property('fixed_dt'),
            iterations=actor.get_editor_property('contact_iterations'),
            original_sha256=before,
            level_sha256=hashlib.sha256((root/'Content/ProcessPlant_CUDA_20261118/L_ProcessPlant_CUDA_1118.umap').read_bytes()).hexdigest())
(root/'Docs/CUDA20261118/level_configuration.json').write_text(json.dumps(report,indent=2))
unreal.log('CUDA_LEVEL_CONFIGURED '+json.dumps(report))
