"""Assemble the implementation receipt from measured evidence, without git writes."""
import hashlib,json,pathlib,subprocess
root=pathlib.Path(__file__).resolve().parents[1];out=root/'Docs/Demo20261119'
def read(name):return json.loads((out/name).read_text(encoding='utf-8-sig'))
files=['Source/VirtualGameTeamProj/PowderPlantTest.cpp','Source/VirtualGameTeamProj/PowderPlantTest.h','Source/VirtualGameTeamProj/PowderPlantRuntime.inl','Source/VirtualGameTeamProj/PowderPlantDemo.inl','Physics/PowderCore/include/powder/demo_plant.hpp','SourceAssets/ProcessPlant_Demo20261119/rotor_boxes.txt','Content/ProcessPlant_Demo20261119/L_ProcessPlant_Demo30FPS_1119.umap']
report=dict(date_kst='2026-11-19',project=str(root/'VirtualGameTeamProj.uproject'),level=read('level.json'),
    hardware=dict(cpu='Intel Core i7-14700KF',gpu='NVIDIA GeForce RTX 4060 Ti 16 GB',driver='610.88',engine='Unreal Engine 5.8',os='Windows 11 26H2',source='demo_final_r0.log'),
    priority='Demonstration: 30 fps and real-time progress before physical fidelity',
    implementation=dict(display_particles=20000,solver_particles=2500,group_size=8,display_radius_m=.002,solver_radius_m=.004,
        time_step_s=1/120,contact_iterations=1,solver='CPU coarse soft contacts; source-derived constrained pipe transport',
        rendering='GPU instanced static meshes, per-frame publication; copy state under lock, build transforms outside lock',
        dormant_state='Closed/no-forward-flow zones rest after 0.5 s without arrivals. Arrivals/effective discharge/RPM wake them. Presentation approximation, not physical convergence.',
        rotor='Conservative source-mesh swept envelope plus RPM-driven circulation; individual paddle impacts omitted',
        mass_note='8 equal-density grains per representative sphere preserve nominal mass/volume, about 0.804247719 kg total. No actual material calibration.'),
    acceptance=read('acceptance_summary.json'),contracts=read('contracts_sleep.json'),original_mesh_sampling=read('mesh_audit_sleep.json'),
    earlier_trials=read('pre_timer_acceptance_summary.json'),
    hidden_window_timing=dict(before='Third five-minute trial: 25.505874522 fps, rate 0.999913442; rendering/physics under frame budget, but hidden-window timer waiting overshot.',
       treatment='Only the own offscreen benchmark process honors Unreal timer resolution using ProcessPowerThrottling. No engine code, OS power plan, GPU clock, or user editor changes.',
       source='https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessinformation',
       lighting_probe=read('render_lite_probe.json'),timer_probe=read('timer_probe.json')),
    excluded_candidates=[dict(name='rendered_probe01',reason='Engine reduced actual output resolution to 888x500'),dict(name='acceptance_r0 and final_r0',reason='Dense hopper latency observed; own runs stopped for optimization. Aborted results are not acceptance passes.')],
    core_screening=read('core_sleep.json'),core_screening_note='Solver-only screening, not end-to-end fps or a fair same-model speedup against OpenCL. Some earlier candidate runs overlapped compilation.',
    use_ko=['열려 있는 기존 에디터를 저장 후 재시작하고 새 레벨 L_ProcessPlant_Demo30FPS_1119를 열어 Play.',
       '기본 자동 시연: 0~2초 대기, 2~22초 공급, 22~48초 이송, 48초 이후 배출·혼합.',
       '수동 조작은 자동 시연을 해제한 뒤 사용. 밸브/혼합기 버튼은 자동 시연을 해제한다.',
       '1~4 장치 확대, Space 일시정지, R 초기화. 원본 OpenCL/CUDA 레벨은 시연 모드를 사용하지 않는다.'],
    limitations=['Not DEM or experimentally validated material physics','Transport does not predict clogging or particle-scale pipe collisions','Bulk resting is a deliberate approximation and can suppress settling detail','Triangle audit samples five-second states; not an exhaustive swept collision proof','Rendered offscreen game trials, not hardware display presentation timing or packaged shipping performance'],
    source_hashes={p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in files},
    git_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),commits_created=False,worktrees_created=False)
if (out/'installation.json').exists():report['installation']=read('installation.json')
for name in ['original_opencl_smoke.json','original_cuda_smoke.json','installed_smoke.json']:
    if (out/name).exists():report[name]=read(name)
(out/'RESULTS.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
print('Wrote',out/'RESULTS.json')
