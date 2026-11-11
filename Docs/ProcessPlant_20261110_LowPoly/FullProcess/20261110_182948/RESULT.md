# 전체 공정 실제 실행 기록

상태: 종료

새 저폴리 레벨의 실제 UE PIE에서 공급기 A/B 각각 10000개, 지름 4mm로 시작했다. 중간 입자 주입·삭제·텔레포트·크기 변경 없이 동일 입자를 추적한다.

현재 단계: mix_15RPM; 마지막 위치 기록의 물리 시간: 38.265279s; 전체 실제 시간: 12276.5s.
레벨 로딩: 0.860s; 레벨 로딩 시작부터 PIE 준비까지: 2.188s.

## 현재 입자 위치

| 영역 | A | B |
|---|---:|---:|
| MX | 9469 | 9621 |
| RH_to_MX_lumen_envelope | 290 | 358 |
| Supply_to_WH_pipe | 0 | 1 |
| WH | 241 | 20 |

전체 ID와 종별 합계를 매 스냅샷에서 검사한다. 공급기 영역은 실제 출구 높이부터 원통부까지 포함한다. 관로는 계산용 공기 통로의 기하학적 근접 판정이다. RH_to_MX_lumen_envelope는 원본 연결관 단면으로 얻은 축대칭 내경 범위이고, RH_to_MX_boundary_tolerance는 내경 바깥 2.5mm 이내의 경계 확인 대상이다. 둘 다 정확한 메시 내외부 판정을 대신하지 않는다. `Unclassified_not_proven_leak`는 누출 판정이 아니다. `Outside_conservative_plant_envelope`는 넓게 잡은 설비 외곽 범위 밖이다.

**원래 `progress.csv`의 Supply/Other는 원뿔부를 빠뜨린 초기 분류이므로 결과 판정에 사용하지 않는다. 보정본 `classified_progress.csv`, `classified_status.json`, `classified/`를 사용한다. 원본은 추적성을 위해 보존한다.**

## 단계 결과

| 단계 | 결과 | 물리 경과(s) | 실제 경과(s) | 종료 위치 파일 |
|---|---|---:|---:|---|
| A_to_WH | completed_all_arrived | 1.680320 | 782.953 | 00026_A_to_WH.csv |
| B_to_WH | residual_stopped_observed_long_stagnation_criterion_not_met | 2.480640 | 2805.625 | 00121_B_to_WH.csv |
| WH_to_RH | residual_stopped_observed_long_stagnation_criterion_not_met | 4.653920 | 1189.078 | 00162_WH_to_RH.csv |
| RH_to_MX | incomplete_terminal_inflow_procedure_transition_long_stagnation_not_met | 18.953440 | 5471.093 | 00345_RH_to_MX.csv |

각 이송의 완료 기준은 대상 전량 도착 후 0.5물리초 유지이다. 최소 10물리초 실행 후 10물리초 동안 목적지 최고 개수가 10개 이상 증가하지 않으면 정체 실패로 기록하고, 실제 도착한 입자로 다음 단계를 검사한다. 최종 혼합은 15RPM·12물리초, 종별 99% 이상 혼합기 도착 및 조성 분산 ≤0.025를 마지막 4초 유지해야 통과한다. 한 단계라도 실패하면 전체 공정 완료가 아니다.

## 압력과 밸브

압력 단위 Pa(g), 대기압 0Pa(g)=101325Pa(abs). 절대압=101325+게이지압. 시험 모델 값이며 실제 설비 안전치가 아니다. 공급 A 충전/A만 개방 및 B 충전/B만 개방: +500/-500/0. 칭량→수취: 공급 닫기·칭량 열기·수취 닫기, 0/+500/-1000. 수취→혼합: 공급·칭량 닫기·수취 열기, 0/0/0. 혼합: 모든 출구 닫기, 0/0/0.

설정과 적용값, 밸브 요청/실제, 유량은 아래 런타임 CSV에 있다. 닫힘은 입자 점유에 따라 지연될 수 있다. 도착 영역의 좌표 판정과 메시 내부 판정을 동일시하지 않는다.

런타임 로그: `D:\GitHub\VirtualGameTeamProj\Saved\PlantPressureRuns\20261110T092949Z_DE0C5DF345EC5CD629E2368A2E826908.csv`

마지막 런타임 기록: t=38.2575990335s, 준비=0.5229746s, 활성 계산=12225.5338512s, 준비 후 실제=12272.6121504s.
압력 설정 S/W/R=0/0/0; 적용=0/0/0 Pa(g).
공급 마스크 요청/적용=0/0, 칭량=0/0, 수취=0/0, RPM=15.

## 실행 오류

Traceback (most recent call last):
  File "full_process_revised_controller", line 19, in tick
  File "full_process_cohort_controller", line 3, in sample_with_cohort
  File "D:/GitHub/VirtualGameTeamProj/Scripts/run_full_process_20261110.py", line 71, in sample
    report['latest']=result;report['run_log']=str(runtime);save_report();last_wall=wall;last_sim=sim
                                                           ^^^^^^^^^^^^^
  File "D:/GitHub/VirtualGameTeamProj/Scripts/run_full_process_20261110.py", line 20, in save_report
    tmp=OUT/'status.tmp';tmp.write_text(json.dumps(report,indent=2),encoding='utf8');tmp.replace(OUT/'status.json')
                                                                                     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\Lib\pathlib.py", line 1188, in replace
    os.replace(self, target)
PermissionError: [WinError 5] 액세스가 거부되었습니다: 'D:\\GitHub\\VirtualGameTeamProj\\Docs\\ProcessPlant_20261110_LowPoly\\FullProcess\\20261110_182948\\status.tmp' -> 'D:\\GitHub\\VirtualGameTeamProj\\Docs\\ProcessPlant_20261110_LowPoly\\FullProcess\\20261110_182948\\status.json'


## 명시적으로 변경된 시험 절차

전체 경로 검사 요청을 이행하기 위한 시험 절차 조정으로 B 잔류 1개는 장기 정체 기준 미충족으로 기록하고, 실제 도착한 19999개로 후속 경로를 검사한다. 칭량→수취 단계에서도 잔류261개 및 상류1개를 보존하고 실제 수취 도착19738개로 후속 검사한다. 수취→혼합 단계 역시 말기 소량 유입을 미완료로 기록하고 실제 도착19002개(A9396/B9606)로 혼합을 검사한다. 세 단계는 위 기본 10초 정체 기준의 예외이며 몰래 완료 처리한 것이 아니다. ID19261을 포함한 총 20000개는 계속 계산한다. 이후 단계는 해당 단계 시작에 실제 도착한 입자 ID 집합을 별도 파일로 보존하고 그 집합의 도착을 검사한다. 전체 공정 성공 판정과 부분 도착분 검사는 다르다.
전환 시각 UTC 2026-11-10T10:29:38.980640+00:00, 물리 시간 4.160960s, PID 17164, 초기화 False, 물리 조건 변경 False. 단계 1→2, 대상 19999개. 자세한 설정/밸브 요청은 protocol_change*.json 참조.
전환 시각 UTC 2026-11-10T10:49:28.054050+00:00, 물리 시간 8.814880s, PID 17164, 초기화 False, 물리 조건 변경 False. 단계 2→3, 대상 19738개. 자세한 설정/밸브 요청은 protocol_change*.json 참조.
전환 시각 UTC 2026-11-10T12:20:39.211013+00:00, 물리 시간 27.768319s, PID 17164, 초기화 False, 물리 조건 변경 False. 단계 3→4, 대상 19002개. 자세한 설정/밸브 요청은 protocol_change*.json 참조.

## 시간·누락 해석

위치와 적용 상태는 비동기 스냅샷이므로 서로 약간 다른 물리 시점일 수 있다. 도착 시각은 첫 관측 시각이며 직전 기록과 해당 기록 사이에 실제 통과가 일어났다. 각 입자의 ID/종/xyz/물리 시각을 원본 및 분류 CSV에 보존한다. 최초 도착/전체 도착 여부는 기록 해상도에 맞춰 해석한다. 중간 중단·초기화 없이 실행 중이며, 실패/종료 사유는 status.json에 기록한다. 준비 시간은 계산 시간과 분리한다.

전체 판정: 실패/미완료 — 단계 결과 참조
최종 혼합 관측: 0.0034327641673519045
