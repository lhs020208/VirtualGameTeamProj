from pathlib import Path
import json,csv,hashlib,shutil,math
import numpy as np
from scipy.spatial import cKDTree
r=Path('D:/GitHub/VirtualGameTeamProj');out=r/'Docs/DEM/optimization45';evidence=out/'render';evidence.mkdir(exist_ok=True)
renders={}
for label in ['ref50','fast50','fast100','final50_long','final100','final_mx50']:
 source=r/'Saved'/('Opt45_'+label);destination=evidence/label;destination.mkdir(exist_ok=True)
 for name in ['result.json','settings.json','trace.csv','final.bin','mix.png','discharge.png']:
  if (source/name).exists():shutil.copy2(source/name,destination/name)
 result=json.loads((source/'result.json').read_text(encoding='utf-8-sig'));settings=json.loads((source/'settings.json').read_text());assert not result['error'] and result['time']>=settings['seconds']
 p=np.fromfile(source/'final.bin',dtype=np.float32).reshape(-1,4);assert len(p)==settings['count'] and np.isfinite(p).all() and np.isin(p[:,3],[0,1]).all()
 radius=(.054/(len(p)*4*math.pi/3))**(1/3);pairs=cKDTree(p[:,:3]).query_pairs(2*radius,output_type='ndarray')
 overlap=float(np.maximum(2*radius-np.linalg.norm(p[pairs[:,0],:3]-p[pairs[:,1],:3],axis=1),0).max(initial=0)/radius)
 result.update(settings=settings,final_overlap_radius=overlap,groups=[int((p[:,3]==k).sum()) for k in (0,1)])
 renders[label]=result
geometry=json.loads((out/'geometry_audit.json').read_text())
summary=dict(renders=renders,geometry=geometry,actual_material_calibrated=False,source_geometry_final=False,whole_production_line_validated=False)
(out/'acceptance.json').write_text(json.dumps(summary,indent=2))
def runrow(label,title):
 x=renders[label];return f'| {title} | {x["count"]:,} | {x["settings"]["dt"]} | {x["time"]:.2f}초 | {x["rate"]:.4f}배 | {10/x["rate"]:.1f}초 |'
text='''# 추가 최적화·테스트 혼합기·최대 처리 속도 검증

2026-10-22 · PowderSimulationOptimization · 기준 커밋 f70e98a 이후 작업

## 구현 결과

- 최신 코어를 Editor 대상으로 재빌드했다. 배치 크기는 최대 8스텝에서 설정 가능한 32스텝으로 바꾸고 단일 입자의 배치별 궤적 동등성을 검사했다. 물리 시간 단계를 건너뛰지 않는다.
- 셀별 연속 메모리 배치(카운팅 정렬)를 구현해 비교했으나 5만 입자에서는 격자 구축 비용이 늘어 느려졌다. 해당 코드는 채택하지 않았고 실험 패치와 로그만 남겼다.
- 기본도형 5만 입자의 권장 시간 간격은 0.000375초, 반복 16회다. 0.0005초는 겹침 기준 미달, 0.001초는 배출 중 안전 중단이 발생해 채택하지 않았다.
- `L_MX301_Test`를 별도로 만들었다. 사용자가 지정한 비최종 `혼합기/teamproj3.blend`를 테스트용으로만 참조했다. 원본 blend는 수정하지 않았다.
- 테스트 혼합기는 내부 볼록 표식(133평면)과 회전부 17개 볼록 부품으로 충돌 경계를 구성한다. 회전부 모서리의 구 접촉은 보수적인 평면 확장 근사다. 정확한 삼각형 메시 접촉이나 확정된 생산 설비가 아니다.
- FBX Y축 반전을 내보내기에서 보정했고, Unreal에 들어온 회전부 중심·크기를 원본 로컬 좌표와 비교했다. 관찰을 위해 표시용 용기의 앞부분만 잘랐으며 계산 경계는 닫혀 있다.
- 테스트 혼합기는 **닫힌 교반 시험**이다. 배출·게이트·진공 이송은 이 자산 경로에 구현하지 않았으며 기본도형 장면에서만 중력 배출을 시험했다.

## 같은 입자 수에서 확인한 속도

RTX 4060 Ti / i7-14700KF / RAM 32GB, 1280×720, 화면 30FPS 상한, 모든 입자 표시, 통계 및 좌표/CSV 저장 포함. 요청 배속은 2로 높여 계산 처리 능력을 측정했다. 초기 로딩/커널 컴파일을 제외한 시뮬레이션 운전 중 평균이다. 다른 입자 시뮬레이션은 동시에 실행하지 않았다.

| 장면·조건 | 입자 | 시간 간격(s) | 시험 물리시간 | 실제 1초당 진행 | 시뮬레이션 10초 소요 |
|---|---:|---:|---:|---:|---:|
'''
text+='\n'.join([runrow('ref50','기본도형 기존 간격'),runrow('fast50','기본도형 가속 후보'),runrow('final50_long','기본도형 최종 연장'),runrow('final100','기본도형 10만 조정'),runrow('final_mx50','비최종 혼합기 테스트')])
text+='''

위 값은 이번에 시험한 조건 중 확인된 처리 속도이며 하드웨어의 이론적 최대치가 아니다. 실시간 1배속 또는 전체 생산 공정에서 같은 배속을 보장하지 않는다. 12초 시험과 30초 시험은 동일 길이 비교가 아니다. 접촉 밀집도와 배출·적층 단계에 따라 속도가 변한다.

두 장면 모두 5만 입자는 64.8kg 상당이며 실제 반지름은 약 6.36mm인 대표 입자다. 시험 혼합기의 내부 표식 체적은 약 783L이므로 명목 분말층 90L는 약 11.5%이며 **반 충전이 아니다**. 실제 반 충전·다른 질량은 별도 검증해야 한다.

## 충돌 및 민감도 판정

잠정 수치 기준은 기록된 최대 입자 겹침/반지름 0.1 이하, 표본 용기/날개 침범 0.1mm 이하로 두었다. 실제 분말 정확도 인증 기준이 아니다. 기록은 약 0.25초 간격, 형상 표본은 약 1초 간격과 최종 위치다. 모든 시간 단계의 무관통을 증명한 것은 아니다.

'''
for key,case in geometry['cases'].items():
 text+=f'- {key}: 최대 겹침/반지름 {case["max_pair_overlap_radius"]:.5f}, 용기 {case["max_sampled_vessel_penetration_m"]*1000:.5f}mm, 회전부 {case["max_sampled_rotor_penetration_m"]*1000:.5f}mm, 입자 그룹 보존.\n'
text+='''
5만 0.000375초의 12초 결과는 기존 0.00025초 대비 최종 수취 수 약 1.8% 차이, 평균 높이 약 0.21mm 차이, 혼합 지표 약 0.0039 차이였다. 원자적 이웃 삽입과 비선형 접촉 때문에 개별 궤적의 비트 단위 일치나 분말 실험과의 동등성을 뜻하지 않는다. 10만의 0.000375초 조건은 속도는 통과했지만 겹침 약 0.103으로 기준을 조금 넘어 0.00035초로 재시험했다.

## 물성 보정의 현재 범위

자료에는 해당 가상 입자를 보정할 실측 곡선이 없다. 마찰 0.2/0.4/0.6의 동일 기준 시험과 측정 입력 CSV 및 후보 적합 도구를 만들었다. 12초 수취량은 아래와 같다.

'''
for mu,label in [(.2,'friction02'),(.4,'reference12'),(.6,'friction06')]:
 row=list(csv.DictReader((out/label/'trace.csv').open()))[-1];text+=f'- 마찰 {mu}: {float(row["receiver_count"])*64.8/50000:.3f}kg 상당\n'
text+='''
`Scripts/calibrate_granular_material.py`는 측정값·허용 오차로 후보를 비교한다. `material_measurements.csv`는 의도적으로 비어 있다. `synthetic_tool_selfcheck.csv`는 도구 검사용 가상 데이터이며 실측 자료가 아니다. 현재 상태는 awaiting_measured_data이고 실제 물성 보정은 완료하지 않았다. 이 모델은 회전 자유도·응집·항력·파쇄·입도분포를 지원하지 않으므로 마찰 하나로 모든 실물 거동을 맞출 수 없다.

## 검증 및 사용

- 코어 회귀: `ctest.log` 참조. Unreal 기존 자동 검증: `unreal_tests/index.json` 참조.
- `L_GranularMixer`: 기존 배출 실험장. 기본 5만·dt 0.000375·목표 배속 0.2.
- `L_MX301_Test`: 비최종 장비의 별도 닫힌 혼합 실험. 기본 5만·dt 0.000375·목표 배속 0.18.
- TargetRate는 요청 속도다. 처리 능력을 넘기면 실제 배속이 낮아지며 시간을 건너뛰거나 녹화 영상을 재생하지 않는다.
- 마찰/입자 수/시간 간격을 바꾸면 ResetSimulation으로 다시 시작한다. 측정은 각 조건에서 따로 해야 한다.

검증 세부 수치: `optimization45/acceptance.json`, `geometry_audit.json`, 각 trace와 렌더 이미지. 중간 좌표 표본은 Saved/Optimization45Snapshots에 보관하며 snapshot_archive.json에 해시를 기록했다. 이번 작업은 커밋하지 않았다.
'''
name='추가최적화_테스트혼합기_및_가속성능_결과.md'
(r/'Docs/DEM'/name).write_text(text,encoding='utf-8');Path('D:/GitHub/가상현실게임',name).write_text(text,encoding='utf-8')
manifest={str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in list((r/'Physics/PowderCore/src').glob('granular*'))+[r/'Source/VirtualGameTeamProj/PowderGranularMixer.cpp',r/'SourceAssets/Granular/MX301_convex.json']}
(out/'source_hashes.json').write_text(json.dumps(manifest,indent=2))
print(json.dumps({k:v['rate'] for k,v in renders.items()},indent=2))
