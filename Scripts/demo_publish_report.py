import hashlib,json,pathlib,subprocess
root=pathlib.Path(__file__).resolve().parents[1];out=root/'Docs/Demo200kSetup'
def read(name):return json.loads((out/name).read_text(encoding='utf-8-sig'))
originals=json.loads((root/'Docs/Demo20261119/original_hashes.json').read_text())
preserved={p:hashlib.sha256((root/p).read_bytes()).hexdigest()==h for p,h in originals.items()}
assert all(preserved.values())
installed=read('verify_installed.json')
assert installed['configuration_pass'] and not installed['error'] and installed['display_particles']==200000 and installed['solver_particles']==2500
assert .98<=installed['rate']<=1.02 and installed['MX']==20000
installation=read('installation.json')
assert hashlib.sha256(pathlib.Path(installation['module']).read_bytes()).hexdigest()==installation['sha256']
removed=set(read('cleanup.json')['removed_empty_directories'])
if (out/'cleanup_initial.json').exists():removed.update(read('cleanup_initial.json')['removed_empty_directories'])
files=['Source/VirtualGameTeamProj/PowderPlantTest.cpp','Source/VirtualGameTeamProj/PowderPlantTest.h','Source/VirtualGameTeamProj/PowderPlantDemo.inl','Physics/PowderCore/include/powder/demo_plant.hpp','Content/ProcessPlant_Demo20261119/L_ProcessPlant_Demo30FPS_1119.umap']
result=dict(level=read('level.json'),installation=installation,
    configuration=dict(display_particles=200000,solver_particles=2500,physical_input_particles=20000,visual_copies=10,physical_mass_unchanged=True,incremental_instance_updates=True,reused_transform_buffers=True),
    cleanup=dict(removed_empty_directories=sorted(removed),removed_count=len(removed),asset_files_deleted=0,kept_top_level_folders=sorted(p.name for p in (root/'Content').iterdir() if p.is_dir()),
       shared_materials=read('asset_audit.json')['old_material_referencers'],note='Collections and Developers can be recreated empty by Unreal startup; old unused dated folders were removed.'),
    verification=dict(staged=read('verify_staged.json'),installed=installed,opencl=read('original_opencl.json'),cuda=read('original_cuda.json'),original_levels_and_inputs=preserved),
    source_sha256={p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in files},
    usage_ko='기존 에디터를 재시작한 뒤 L_ProcessPlant_Demo30FPS_1119 레벨을 열어 실행. 기존 실행 중인 에디터는 핫리로드하거나 종료하지 않았음.',
    limitations=['200,000 visual markers, not 200,000 independent physical particles','Measured in offscreen game mode with the original editor left running; no claim of 30fps acceptance','Original OpenCL and CUDA checks are short launch/advance compatibility tests'],
    git_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),commits_created=False,new_git_worktrees=False)
(out/'RESULTS.json').write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf-8')
print('Verified deployed counts:',installed['display_particles'],installed['solver_particles'],'FPS',installed['fps'],'rate',installed['rate'])
print('Removed empty folders:',len(removed),'original assets preserved:',all(preserved.values()))
