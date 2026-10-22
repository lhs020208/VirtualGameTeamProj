import json
from pathlib import Path
root=Path('D:/GitHub/VirtualGameTeamProj')
out=root/'Docs/DEM/optimization_followup'
a=json.loads((out/'audit.json').read_text())
b,c,d=(a['cases'][x] for x in ('before','after','long50'))
text=f'''# GPU 입자 추가 최적화 및 연장 검증

2026-10-22 · PowderSimulationOptimization · 커밋/푸시하지 않음

## 변경

- 이웃 후보 거리의 제곱을 먼저 비교하고, 실제 접촉 후보만 제곱근 계산한다. 이웃을 생략하거나 접촉 반복 횟수를 줄이지 않았다.
- 코어 실행 도구의 혼합도에서 관·수취부 입자를 제외했다. 에디터와 같은 혼합기 내부 영역(z >= 0)의 지표를 사용한다. 빈 공간/단일 성분은 -1이다. 이전 기록의 배출 후 혼합도와 직접 비교하면 안 된다.
- 별도 audit_granular_followup.py로 좌표 유한성, 입자 수, 그룹, 최종 외벽 침범·겹침, 혼합도 독립 재계산을 검증했다.

## 비교 시험

3만 입자, 4초, dt=0.00025초, 접촉 16회, 30RPM, 게이트 닫힘, GPU 프로파일링 사용.
에디터가 함께 실행된 공유 GPU 측정이며 전용 성능 인증이 아니다. 렌더링을 포함한 새 인수 시험도 아니다.

| 지표 | 변경 전 | 변경 후 |
|---|---:|---:|
| 코어 실행·검사·저장 포함 배속 | {float(b['last']['rate']):.4f} | {float(c['last']['rate']):.4f} |
| GPU 접촉 커널 누계(초) | {b['timing']['contact_gpu_s']:.4f} | {c['timing']['contact_gpu_s']:.4f} |
| advance 벽시간(초) | {b['timing']['advance_wall_s']:.4f} | {c['timing']['advance_wall_s']:.4f} |

평균 높이 차이 {a['comparison']['mean_height_difference_m']*1000:.4f}mm, 혼합도 차이 {a['comparison']['mix_difference']:.6f}. 입자 인덱스별 그룹 동일. 원자적 격자 삽입과 수치 연산 변화 때문에 궤적의 비트 단위 동일성을 요구하지 않는다. 단일 비교로 일정한 향상률을 보장하지 않는다.

## 5만 입자 연장 시험

물리시간 {float(d['last']['time']):.3f}초까지 실행(기존 12초보다 연장). 1초부터 30RPM, 9초에 게이트 개방.
총 5만 개 유지, 최종 수취 {d['last']['receiver_count']}개({d['receiver_mass_kg']:.3f}kg 상당), 혼합기 잔류 {d['remaining_mixer_count']}개.
관 내부 입자는 수취나 혼합기 잔류와 구분된다. 완전 배출에 도달한 시험이 아니다.

- 기록된 최대 겹침/반지름: {d['max_recorded_overlap_radius']:.6f}
- 최종 스냅샷 최대 겹침/반지름: {d['final_overlap_radius']:.6f}
- 최종 외벽 침범: {d['final_outer_wall_intrusion_m']:.9f}m
- 최종 혼합도 독립 재계산: {d['independent_mixing_index']:.6f}
- 모든 기록 유한, 해석기 오류 없이 종료. 검사는 약 0.25초 간격과 최종 좌표이며 모든 시간 단계·모든 내부 날개 표면의 무관통 증명은 아니다.
- 코어 실행 중 다른 회귀 테스트 및 에디터가 자원을 공유했다. 이번 배속을 이전 화면 포함 인수 수치와 비교하지 않는다.

## 빌드와 회귀

전체 CTest 48/48 통과. Unreal 게임 대상 Development 빌드 성공. 현재 에디터를 닫거나 교체하지 않아 실행 중인 에디터 DLL은 이전 버전이다. 변경 커널은 코어 실행 파일과 게임 대상에 컴파일했으며 에디터 반영은 에디터 종료 후 Editor 대상 재빌드가 필요하다.

## 남은 한계

10만 입자의 화면 포함 0.1배속 달성을 새로 입증하지 않았다. 실물 물성/실제 기계 형상 검증, 좁은 간극의 대표 입자 오차, 유체 항력, 응집 및 비구형 효과는 그대로 남는다. 다음 큰 성능 개선 후보는 메모리 접근이 연속되도록 격자·입자 배열을 정렬하는 방법이며 별도 결과 동등성 검증이 필요하다. 이번 변경만으로 전체 공정 정확도나 속도 목표가 해결되었다고 판정하지 않는다.

재현 자료: `optimization_followup/audit.json`, 각 실행의 `trace.csv`, `final.bin`, `timing.json`, `ctest.log`, `unreal_build.log`. 비교 전 바이너리는 Saved/granular_followup_baseline.exe, 해시는 baseline_hash.txt.
'''
name='GPU입자_추가최적화_및_연장검증.md'
(root/'Docs/DEM'/name).write_text(text,encoding='utf-8')
Path('D:/GitHub/가상현실게임',name).write_text(text,encoding='utf-8')
print(text)
