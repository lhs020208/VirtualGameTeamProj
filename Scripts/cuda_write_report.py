"""Generate the report from measured results, preserving failed trials separately."""
import json
import pathlib

root=pathlib.Path(__file__).resolve().parents[1]
out=root/'Docs/CUDA20261118'
summary=json.loads((out/'benchmark_final/summary.json').read_text())
metrics=json.loads((out/'benchmark_final/state_metrics.json').read_text())
smoke=json.loads((out/'unreal_smoke_result.json').read_text())
integrity=json.loads((out/'integrity.json').read_text())
validation=json.loads((out/'validation_graph_memset.json').read_text())
names={'initial':'초기 공급 배치','transfer':'밀집 이송 재시작','mixing':'15 RPM 혼합 재시작'}
def link(label,path):return f'[{label}]({(root/path).as_posix()})'
rows=[];ranges=[];state=[]
for case in summary['results']:
    stages=case['stages'];name=names[case['scene']]
    rows.append('| '+name+' | '+' | '.join(f"{s['median_wall_s']:.6f}" for s in stages)
                +f" | {case['cuda_throughput_gain_pct']:+.2f}% | {case['optimization_throughput_gain_pct']:+.2f}% | {case['overall_throughput_gain_pct']:+.2f}% |")
    ranges.append('| '+name+' | '+' | '.join(f"{s['min_wall_s']:.6f}–{s['max_wall_s']:.6f}" for s in stages)+' |')
    for c in case['state_comparisons']:
        state.append(f"| {name} | {c['label']} | {c['position_max_m']*1000:.6f} | {c['position_rms_m']*1000:.6f} | {c['velocity_rms_m_s']:.6f} |")
text=f'''# CUDA 전환 및 최적화 결과 — 2026-11-18

원본 프로젝트를 보존한 별도 작업 폴더에서 새 레벨을 만들고 **CUDA Driver 실행 + CUDA Graph + 격자 메모리 초기화 최적화**를 구현했다. 현재 공정 계산식을 유지했다. **실재료 현실성 검증과 힘 기반 DEM으로의 전환은 완료된 것으로 표시하지 않는다.** 기존 모델의 시간 간격 민감도가 확인되어, CUDA 이식과 현실성 확보를 분리해서 판단해야 한다.

프로젝트: {link('VirtualGameTeamProj_CUDA_20261118', 'VirtualGameTeamProj.uproject')}  
새 레벨: `/Game/ProcessPlant_CUDA_20261118/L_ProcessPlant_CUDA_1118`  
이 별도 프로젝트의 시작 레벨도 새 레벨로 설정했다. 기존 프로젝트·레벨은 변경하지 않았으며 커밋하지 않았다.

## 동일 조건 성능 실측

하드웨어는 Intel Core i7-14700KF, RTX 4060 Ti 16 GB, NVIDIA 드라이버 610.88, RAM 32 GB다. 모든 단계에서 입자 20,000개(A/B 각 10,000), 반지름 2 mm, 밀도 1,200 kg/m³, Δt=10 μs, 접촉 반복 8회, 배치 16단계, 벽면 모드 5를 유지했다. 같은 시작 상태에서 **3,200단계 = 물리시간 약 0.032초**를 측정하고 세 번 반복했다. 비교 순서는 ABC/BCA/CAB로 교차했다.

단계 ①은 수정 전 물리 소스 184개 파일의 해시를 검증한 별도 OpenCL 실행 파일이다. 단계 ②는 최적화 전 CUDA 실행 파일을 따로 고정했다. 단계 ③에만 Graph와 동일 값의 격자 메모리 초기화를 적용했다. 각 실행의 명령, 바이너리 SHA-256, 입력 해시, GPU 상태 기록을 보관했다.

아래 시간은 **계산부의 실제 경과 시간(초), 세 번의 중앙값**이다. 초기화·컴파일·워밍업·최종 파일 저장과 렌더링은 제외했다. CUDA 효과는 ①→②, 추가 최적화 효과는 ②→③으로 각각 계산했다.

| 구간 | ① 기존 OpenCL, 초 | ② CUDA만, 초 | ③ 추가 최적화, 초 | CUDA 처리속도 개선 | 추가 최적화 개선 | 전체 처리속도 개선 |
|---|---:|---:|---:|---:|---:|---:|
{chr(10).join(rows)}

처리속도 개선율 = `(이전 시간 / 이후 시간 − 1) × 100`. 시간 감소율은 이와 다르며 원시 요약 JSON에 별도로 저장했다. 구간별 소요시간·대표성이 다르므로 이 표를 임의로 평균해 공정 전체 개선율로 표시하지 않는다.

| 구간 | ① 최소–최대, 초 | ② 최소–최대, 초 | ③ 최소–최대, 초 |
|---|---:|---:|---:|
{chr(10).join(ranges)}

CUDA 전환 자체의 작은 차이와 밀집 이송의 작은 개선율은 반복 변동과 함께 보아야 한다. 밀집 이송에서는 최적화 전 CUDA의 호스트 제출 약 0.25초에 비해 GPU 완료 대기가 약 32.8초였다. API 제출 비용을 줄여도 이 구간의 지배적인 접촉·벽면 계산 비용은 유지된다.

기존 사용자 에디터와 다른 프로세스는 종료하지 않았다. 따라서 완전히 전용으로 격리한 GPU 측정은 아니다. GPU 상태 표본은 프로세스 초기화 구간도 포함하며, 고정 클록 실험으로 표현하지 않는다. 모든 성능 비교는 렌더링이 없는 같은 실행 방식이다. 아래 Unreal 시험의 속도는 이 표와 직접 비교하지 않는다.

### 시작 상태와 제어 조건

초기 배치는 현재 레벨의 `test_charge.csv`를 그대로 사용했다. 밀집 이송·혼합 배치는 기존 위치 CSV에서 읽어 동일한 FP32 상태 파일로 고정했다. 과거 CSV에는 속도가 없어 **속도 0, 회전각 0으로 시작한 통제된 재시작 시험**이다. 완전한 공정 체크포인트 재생이나 실재료 실험이 아니다.

같은 구간의 세 단계에는 같은 밸브·공기 조건을 적용했다. 수동 압력차 1,000 Pa, 공급 A/B 개방을 사용하며, 초기 배치는 칭량/수취 배출을 닫고, 이송 배치는 칭량 배출만 열고, 혼합 배치는 두 배출을 열고 15 RPM으로 회전한다. 레벨의 대기 화면에 저장된 압력 0 조건과 이 통제된 성능 시험 조건을 혼동하지 않는다.

근거: {link('실측 요약', 'Docs/CUDA20261118/benchmark_final/summary.json')}, {link('재시작 상태 출처', 'Docs/CUDA20261118/fixtures.json')}, {link('원본·실행 파일 무결성', 'Docs/CUDA20261118/integrity.json')}.

## 정확성 시험과 한계

- 벽면 표면 표본 19,924개 × 회전각 3개 × 접촉 반경 2개 = **{validation['queries']:,}건**: CUDA와 OpenCL의 위치·법선·벽 속도 차이 0.
- 밀집 이송·혼합 상태의 1단계/16단계 비교: 최종 채택 경로의 입자 위치·속도 차이 0. 밸브·RPM 변경 및 상태 초기화 시험도 수행했다.
- 모든 성능 실행에서 입자 수 20,000개, A/B 각 10,000개, 유한 위치·속도를 확인했다. 입자 ID는 벡터 인덱스를 유지한다. 물리 입력에 따른 총질량은 약 0.804247719 kg이다.
- 기존 CPU 기준판의 Hertz, 마찰, 회전저항, 부착, 메시, 이동벽, 시간 간격, 공기·압력 관련 9개 시험을 통과했다. 이는 해당 CPU 기능의 회귀 시험이며, 현재 공정 GPU가 힘 기반 DEM이라는 뜻은 아니다.
- 전 입자쌍의 매 시간 단계 접촉 누락을 별도의 독립 오라클로 모두 감사한 것은 아니다. 원본 접촉 탐색·처리 순서를 보존한 이식과 상태 비교의 결과다.

원본도 밀집 계산을 반복하면 개별 입자 궤적이 달라진다. 원자적 격자 삽입과 FP32 누산 순서에 따른 비결정성을 고려해야 한다. **0.032초 후 모든 개별 궤적이 같다고 주장하지 않는다.** 아래는 각 구간 첫 실행끼리 비교한 차이이며, 기준 반복은 OpenCL 1회차와 2회차의 차이다.

| 구간 | 비교 | 최대 위치 차이, mm | 위치 RMS 차이, mm | 속도 RMS 차이, m/s |
|---|---|---:|---:|---:|
{chr(10).join(state)}

질량·종류, 질량중심, 운동에너지, 입자 겹침, 호퍼 잔류 수는 별도로 FP64 집계했다. 두 측정 묶음의 변경 없는 OpenCL 총 6회와 모든 CUDA 반복을 짝지은 {link('전체 반복 변동 분석', 'Docs/CUDA20261118/repeatability.json')}도 남겼다. 이 값들의 유사성을 실측 정확도 인증이나 장시간 궤적 일치로 확대 해석하지 않는다. 단기 이식 검증은 통과했지만 장시간 통계적 동등성의 포괄적인 인증은 남아 있다.

근거: {link('최종 경계 검증', 'Docs/CUDA20261118/validation_graph_memset.json')}, {link('밀집 단기 비교', 'Docs/CUDA20261118/short_contact_final/comparison.json')}, {link('집계·겹침 진단', 'Docs/CUDA20261118/benchmark_final/state_metrics.json')}, {link('CPU 시험', 'Docs/CUDA20261118/cpu_reference_tests.xml')}.

### 현실성 목표에 대한 판단

현재 공정은 위치 기반 반복 보정(PBD)과 단방향 관로 공기 모델이다. 실제 분말의 물성·입도·측정 자료가 없고, 힘 기반 Hertz–Mindlin·접선 이력·회전 자유도·비구형·완전한 3차원 양방향 CFD–DEM이 공정에 연결된 상태는 아니다. CUDA로 실행한다는 사실은 이 모델의 현실성을 높였다는 증거가 아니다.

수정 전 OpenCL에 대해 같은 0.032초를 Δt=10/5/2.5 μs로 계산했다. 혼합 재시작의 평균 속도는 약 **0.009172/0.014955/0.040782 m/s**로 변했다. 이송은 약 **0.609623/0.581637/0.578277 m/s**였다. 현재 모델의 수렴을 확인한 결과가 아니며, 더 작은 시간 간격을 자동으로 더 정확한 설정이라고 채택하지 않았다. 속도를 위해 시간 간격·반복 횟수·반지름을 변경하지도 않았다.

따라서 **제1 목표인 실재료 현실성 확보는 미완료**다. 힘 기반 기준판과 공정 형상을 연결하고 접촉 이력·회전·정밀도를 검증한 뒤, 실제 재료의 보정 및 독립 검증이 필요하다. 이번에 완료한 범위는 현재 알고리즘의 CUDA 실행 경로, 보수적인 최적화, 원본 보존, 비교 측정과 한계 확인이다. 자세한 물리 개발·보정 항목은 {link('계획과 현실성 감사', 'Docs/CUDA20261118/PLAN_AND_REALISM.md')}에 남겼다.

근거: {link('시간 간격 민감도', 'Docs/CUDA20261118/convergence/summary.json')}.

## CUDA 구현 방식과 제외한 후보

채택 경로는 CUDA Driver API가 직접 메모리·스트림·커널·동기화를 관리하며 실행 중 계산을 OpenCL에 위임하지 않는다. 현재 검증된 GPU 산술을 유지하기 위해 NVIDIA OpenCL 컴파일러의 PTX를 빌드 아티팩트로 보존했다. PTX의 재생성에는 NVIDIA OpenCL 컴파일러를 사용한다. **CUDA C++ 소스까지 완전히 전환해 검증을 마친 상태와는 다르다.** 현재 아티팩트는 이 RTX 4060 Ti(sm_89)와 드라이버에서 검증했다.

CUDA C++ 커널 후보도 구현했으나 밀집 단일 단계의 최대 속도 차이가 약 0.006 m/s로 사전 허용치 0.002 m/s를 넘었다. 기본 경로에서 제외했고 실패 자료를 보존했다. 빠른 후보를 정확성 통과로 바꾸기 위해 허용치를 완화하지 않았다.

최종 추가 최적화는 CUDA Graph 제출과 `-1` 격자 초기화의 메모리 명령 대체다. 입자 수, 물리식, 벽면 처리 순서, 접촉 반복, 매 반복 격자 재구축, 오류 검사를 유지한다. Graph만 적용했던 중간 비교도 `benchmark_graph/`에 보관한다.

근거: {link('PTX 출처와 재생성', 'Docs/CUDA20261118/ptx_provenance.json')}, {link('CUDA C++ 후보 오차', 'Docs/CUDA20261118/short_contact/comparison.json')}. 관련 공식 문서는 [CUDA Graph](https://docs.nvidia.com/cuda/cuda-programming-guide/04-special-topics/cuda-graphs.html), [NVIDIA 부동소수점](https://docs.nvidia.com/cuda/floating-point/index.html), [PTX 연산 규격](https://docs.nvidia.com/cuda/parallel-thread-execution/)을 참고했다.

## Unreal 실행 확인

UE 5.8 에디터 모듈을 별도 프로젝트에서 빌드했다. 새 레벨의 백엔드는 `CudaPtxGraph`이고, 1280×720 화면 밖 렌더링 시험에서 입자 {smoke['result']['count']:,}개, 물리시간 {smoke['result']['physical_s']:.8f}초, 종료 코드 {smoke['exit_code']}를 확인했다. 이 시험은 기능·렌더링·종료 확인이며 공정 전체 성능 비교는 아니다.

첫 자동 시험에서는 종료 요청 뒤 계산 스레드가 계속 실행되는 문제가 확인됐다. 종료 전에 해당 스레드를 정리하도록 수정하고 재시험했다. 초기 화면이 준비되기 전에 시험이 끝나지 않도록 시작도 지연했다. 검증용 촬영에서는 설비를 숨겨 입자를 관찰한다. 이 촬영 설정은 저장된 레벨의 일반 실행 설정을 바꾸지 않는다.

{link('Unreal 실행 기록', 'Docs/CUDA20261118/unreal_smoke_result.json')}, {link('최종 모듈·CUDA 라이브러리 연결 해시', 'Docs/CUDA20261118/unreal_cuda_link.json')}, {link('검증 화면', 'Docs/CUDA20261118/PlantFlowPreview.png')}.

## 재실행

현재 PC에서는 위 프로젝트 파일을 열면 새 레벨이 시작된다. 원본 프로젝트가 열려 있다면 그 창을 종료하거나 재컴파일하지 말고 별도 프로젝트를 사용한다. 테스트 스크립트는 이 작업 폴더에서 실행한다.

```powershell
python -B Scripts/cuda_verify_sources.py
python -B Scripts/cuda_build.py configure
python -B Scripts/cuda_build.py build cuda_plant_bench cuda_plant_validation
python -B Scripts/cuda_build_unreal.py
python -B Scripts/cuda_unreal_smoke.py
python -B Scripts/cuda_measure.py bench --steps 3200 --folder new_measurement
```

측정 스크립트는 기존 결과를 덮어쓰지 않는다. CUDA-only 비교용 실행 파일은 `Intermediate/CUDAOnly`에 고정되어 있다. 로컬 툴킷을 복구할 때는 `cuda_prepare.py --toolkit-only`를 사용한다. CUDA 드라이버나 시스템 CUDA 설치는 이 작업에서 변경하지 않았다. 원본 레벨과 고정 기준 소스 무결성 확인 결과는 `{str(integrity['pass_integrity']).lower()}`다.
'''
(out/'RESULTS.md').write_text(text,encoding='utf-8')
status=dict(implementation='CUDA Driver + Graph + identical grid reset; isolated level integrated',
            physical_realism_goal='not achieved: experimental data absent and baseline time-step sensitivity remains',
            native_cuda_cpp='implemented prototype; not accepted after accuracy gate failure',
            short_numerical_migration='passed',long_trajectory_identity=False,
            end_to_end_performance_comparison=False,solver_three_stage_comparison=True,
            original_preserved=integrity['pass_integrity'],commits_created=False)
(out/'status.json').write_text(json.dumps(status,indent=2))
print(out/'RESULTS.md')
