import unreal, pathlib, json, hashlib
root=pathlib.Path(unreal.Paths.project_dir()).resolve()
source='/Game/ProcessPlant_20261111_LowPoly/L_ProcessPlant_LowPoly_1111'
dest='/Game/ProcessPlant_CUDA_20261118/L_ProcessPlant_CUDA_1118'
old=root/'Content/ProcessPlant_20261111_LowPoly/L_ProcessPlant_LowPoly_1111.umap'
before=hashlib.sha256(old.read_bytes()).hexdigest()
if not unreal.EditorAssetLibrary.does_asset_exist(dest):
    obj=unreal.EditorAssetLibrary.duplicate_asset(source,dest)
    if not obj: raise RuntimeError('level duplication failed')
    if not unreal.EditorAssetLibrary.save_loaded_asset(obj,False): raise RuntimeError('level save failed')
after=hashlib.sha256(old.read_bytes()).hexdigest()
assert before==after,'original level modified'
new=root/'Content/ProcessPlant_CUDA_20261118/L_ProcessPlant_CUDA_1118.umap'
assert new.exists()
(root/'Docs/CUDA20261118/level_copy.json').write_text(json.dumps({'source':source,'destination':dest,'source_before':before,'source_after':after,'destination_sha256':hashlib.sha256(new.read_bytes()).hexdigest()},indent=2))
unreal.log('CUDA_LEVEL_COPY_VERIFIED')
