"""Check original preservation and frozen-source provenance without changing them."""
import hashlib
import json
import pathlib
import subprocess

root=pathlib.Path(__file__).resolve().parents[1]
out=root/'Docs/CUDA20261118'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
contract=json.loads((out/'contract.json').read_text(encoding='utf-8'))
hashes=json.loads((out/'source_hashes.json').read_text())
failures=[];checked=0
for name,expected in hashes.items():
    prefix='Physics\\PowderCore\\'
    if name.startswith(prefix) and name!=prefix+'CMakeLists.txt':
        path=root/'Intermediate/CUDABaselineSource'/name[len(prefix):]
        checked+=1
        if not path.exists() or sha(path)!=expected:failures.append(str(path))
for project in (root,pathlib.Path(contract['original_project'])):
    path=project/contract['original_level']
    if sha(path)!=contract['original_level_sha256']:failures.append(str(path))
translation=json.loads((out/'kernel_translation.json').read_text())
for name,expected in translation['input_sha256'].items():
    if sha(root/'Physics/PowderCore/src'/name)!=expected:failures.append(name)
if sha(root/'Physics/PowderCore/src/cuda_plant_kernels.cuh')!=translation['output_sha256']:
    failures.append('generated CUDA C++ kernels')
ptx=json.loads((out/'ptx_provenance.json').read_text())
if sha(root/'SourceAssets/PowderCUDA/plant_sm89.ptx')!=ptx['artifact_sha256']:
    failures.append('CUDA PTX artifact')
head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip()
if head!=contract['source_commit']:failures.append('isolated HEAD changed')
records={name:sha(root/path) for name,path in {
    'baseline_executable':'Intermediate/CUDABaseline/cuda_plant_bench.exe',
    'cuda_only_executable':'Intermediate/CUDAOnly/cuda_plant_bench.exe',
    'optimized_executable':'Intermediate/CUDA/cuda_plant_bench.exe',
    'cuda_library':'Intermediate/CUDA/powder_plant_cuda.lib',
    'ptx':'SourceAssets/PowderCUDA/plant_sm89.ptx',
    'runtime_geometry':'SourceAssets/ProcessPlant_20261111_LowPoly/Simulation/runtime.plant',
    'runtime_routes':'SourceAssets/ProcessPlant_20261111_LowPoly/Simulation/runtime.routes',
    'initial_charge':'SourceAssets/ProcessPlant_20261111_LowPoly/Simulation/test_charge.csv',
}.items()}
result=dict(pass_integrity=not failures,failures=failures,frozen_core_files_checked=checked,
            original_level_sha256=contract['original_level_sha256'],source_commit=head,
            artifact_sha256=records)
(out/'integrity.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2))
if failures:raise SystemExit(1)
