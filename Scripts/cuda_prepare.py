"""Task-local preparation; never alters the original project or global CUDA installation."""
import hashlib, json, pathlib, shutil, urllib.request, zipfile, sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / 'Docs/CUDA20261118'
OUT.mkdir(parents=True, exist_ok=True)
toolkit_only='--toolkit-only' in sys.argv
if (OUT/'source_hashes.json').exists() and not toolkit_only:
    raise RuntimeError('Baseline already frozen. Refusing to overwrite evidence; use --toolkit-only to restore the local CUDA toolkit.')
level = pathlib.Path('Content/ProcessPlant_20261111_LowPoly/L_ProcessPlant_LowPoly_1111.umap')
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
contract = {
 'source_commit':'fa028b18d8ce8fb3498bf921fa20167eed541925',
 'original_project':'D:/GitHub/VirtualGameTeamProj', 'isolated_project':str(ROOT),
 'original_level':str(level), 'original_level_sha256':sha(ROOT/level),
 'priority':['physical_validity','numerical_accuracy','performance'],
 'no_commits':True, 'no_other_process_control':True,
 'measurement':{'stages':['OpenCL baseline','CUDA only','CUDA optimized'],
 'count':20000,'radius_m':.002,'density_kg_m3':1200,'dt_s':.00001,'iterations':8,
 'batch_steps':16,'wall_mode':5,'local_size':128,'repeats':3,
 'initial_state_identical':True,'physics_changes_separate':True,
 'startup_and_warmup_excluded':True,'GPU_profile_off_for_throughput':True},
 'validation':{'missing_or_duplicate_ids':0,'missed_contact_candidates':0,
 'nonfinite_values':0,'boundary_position_abs_m':2e-6,
 'boundary_normal_abs':.002,'single_step_position_abs_m':2e-6,
 'single_step_velocity_abs_m_s':.002,
 'note':'Provisional numerical migration gates, not experimental accuracy certification. Long-run tolerances require convergence and baseline-repeat evidence; no post-hoc relaxation.'},
 'real_measurements_available':False,
 'states':{str(i):'pending' for i in range(1,14)}
}
baseline = ROOT/'Intermediate/CUDABaselineSource'
if not toolkit_only:
    (OUT/'contract.json').write_text(json.dumps(contract,ensure_ascii=False,indent=2),encoding='utf-8')
    if not baseline.exists(): shutil.copytree(ROOT/'Physics/PowderCore',baseline)
    files={str(p.relative_to(ROOT)):sha(p) for d in ['Physics/PowderCore','Source','Config'] for p in (ROOT/d).rglob('*') if p.is_file()}
    (OUT/'source_hashes.json').write_text(json.dumps(files,indent=2),encoding='utf-8')
base='https://developer.download.nvidia.com/compute/cuda/redist/'
manifest=json.load(urllib.request.urlopen(base+'redistrib_12.8.1.json'))
toolroot=ROOT/'Intermediate/CUDAToolkit'
toolroot.mkdir(parents=True,exist_ok=True)
for name in ['cuda_nvcc','cuda_cudart','cuda_cccl']:
    item=manifest[name]['windows-x86_64']; archive=toolroot/(name+'.zip')
    print('Downloading',name,flush=True)
    if not archive.exists(): urllib.request.urlretrieve(base+item['relative_path'],archive)
    if sha(archive)!=item['sha256']: raise RuntimeError('download hash mismatch '+name)
    with zipfile.ZipFile(archive) as z:
        for member in z.infolist():
            rel=pathlib.PurePosixPath(member.filename).parts[1:]
            if not rel or member.is_dir(): continue
            dest=toolroot.joinpath(*rel)
            if not dest.resolve().is_relative_to(toolroot.resolve()): raise RuntimeError('unsafe archive')
            dest.parent.mkdir(parents=True,exist_ok=True)
            with z.open(member) as src, dest.open('wb') as dst: shutil.copyfileobj(src,dst)
print('Prepared',toolroot,flush=True)
