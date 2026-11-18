"""Build only this isolated project's editor module, with two compile workers."""
import pathlib
import subprocess
import sys
import hashlib
import json
root=pathlib.Path(__file__).resolve().parents[1]
assert root.name=='VirtualGameTeamProj_CUDA_20261118'
library=root/'Intermediate/CUDA/powder_plant_cuda.lib'
receipt=root/'Docs/CUDA20261118/unreal_cuda_link.json'
library_hash=hashlib.sha256(library.read_bytes()).hexdigest()
old=json.loads(receipt.read_text()) if receipt.exists() else {}
if old.get('cuda_library_sha256')!=library_hash:
    # UBT may reuse its action cache for an externally built static library.
    # Touch only this checkout's adapter to force a fresh link, never hot reload.
    adapter=root/'Source/VirtualGameTeamProj/DEMCore_granular.cpp'
    assert adapter.exists()
    adapter.touch()
build=pathlib.Path('C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat')
args=[str(build),'VirtualGameTeamProjEditor','Win64','Development',str(root/'VirtualGameTeamProj.uproject'),'-WaitMutex','-NoHotReloadFromIDE','-NoLiveCoding','-MaxParallelActions=2','-NoUBA']
log=root/'Docs/CUDA20261118/unreal_build.log'
with log.open('w',encoding='utf-8') as f:
    p=subprocess.Popen(args,cwd=root,stdout=f,stderr=subprocess.STDOUT,creationflags=0x08000000)
    print('Own Unreal build PID',p.pid,flush=True)
    code=p.wait()
print('exit',code,'log',log,flush=True)
print('\n'.join(log.read_text(errors='replace').splitlines()[-18:]))
if code==0:
    dll=root/'Binaries/Win64/UnrealEditor-VirtualGameTeamProj.dll'
    receipt.write_text(json.dumps(dict(cuda_library_sha256=library_hash,
        unreal_module_sha256=hashlib.sha256(dll.read_bytes()).hexdigest()),indent=2))
sys.exit(code)
