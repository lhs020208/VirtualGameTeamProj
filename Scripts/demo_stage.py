"""Disposable build host within Intermediate, never a git worktree.
Content and input assets are shared; binaries and build intermediates are isolated.
"""
import pathlib,shutil,subprocess
root=pathlib.Path(__file__).resolve().parents[1]
host=root/'Intermediate/Demo20261119/EditorHost';host.mkdir(parents=True,exist_ok=True)
for name in ['Source','Config']:
    shutil.copytree(root/name,host/name,dirs_exist_ok=True)
rules=host/'Source/VirtualGameTeamProj/VirtualGameTeamProj.Build.cs'
s=rules.read_text(encoding='utf-8')
for tail in ['third_party/OpenCL','include']:
    s=s.replace('System.IO.Path.Combine(ModuleDirectory, "../../Physics/PowderCore/'+tail+'")','"'+(root/'Physics/PowderCore'/tail).as_posix()+'"')
rules.write_text(s,encoding='utf-8')
shutil.copy2(root/'VirtualGameTeamProj.uproject',host/'VirtualGameTeamProj.uproject')
for name in ['Content','SourceAssets','Docs','Plugins','Physics']:
    link=host/name
    if not link.exists():subprocess.run(['cmd','/d','/c','mklink','/J',str(link),str(root/name)],check=True,capture_output=True)
for relative in ['Intermediate/CUDA/powder_plant_cuda.lib','Intermediate/CUDAToolkit/lib/x64/cudart_static.lib','Intermediate/CUDAToolkit/lib/x64/cuda.lib']:
    target=host/relative;target.parent.mkdir(parents=True,exist_ok=True)
    if not target.exists():shutil.copy2(root/relative,target)
print('Staged source in',host)
