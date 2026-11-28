from full_process_report_io import read_latest_report
"""Maintain a readable report from the running experiment; no simulation controls."""
from pathlib import Path
import json,csv,sys,time,shutil,collections
run=Path(sys.argv[1]);watch='--watch' in sys.argv
def write_report():
 status=read_latest_report(run)[0];classified=json.loads((run/'classified_status.json').read_text());latest=classified['latest']
 lines=['# 전체 공정 실제 실행 기록','',f"상태: {'종료' if 'finished_utc' in status else '실행 중 — 완료 아님'}",'',
 '새 저폴리 레벨의 실제 UE PIE에서 공급기 A/B 각각 10000개, 지름 4mm로 시작했다. 중간 입자 주입·삭제·텔레포트·크기 변경 없이 동일 입자를 추적한다.',
 '',f"현재 단계: {status.get('active_stage')}; 마지막 위치 기록의 물리 시간: {latest['sim_s']:.6f}s; 전체 실제 시간: {status.get('latest',{}).get('wall_s',0):.1f}s.",
 f"레벨 로딩: {status['level_load_wall_s']:.3f}s; 레벨 로딩 시작부터 PIE 준비까지: {status.get('PIE_ready_wall_s',0):.3f}s.",
 '', '## 현재 입자 위치', '', '| 영역 | A | B |','|---|---:|---:|']
 for name,counts in latest['counts'].items():lines.append(f'| {name} | {counts[0]} | {counts[1]} |')
 lines+=['','전체 ID와 종별 합계를 매 스냅샷에서 검사한다. 공급기 영역은 실제 출구 높이부터 원통부까지 포함한다. 관로는 계산용 공기 통로의 기하학적 근접 판정이다. RH_to_MX_lumen_envelope는 원본 연결관 단면으로 얻은 축대칭 내경 범위이고, RH_to_MX_boundary_tolerance는 내경 바깥 2.5mm 이내의 경계 확인 대상이다. 둘 다 정확한 메시 내외부 판정을 대신하지 않는다. `Unclassified_not_proven_leak`는 누출 판정이 아니다. `Outside_conservative_plant_envelope`는 넓게 잡은 설비 외곽 범위 밖이다.',
 '', '**원래 `progress.csv`의 Supply/Other는 원뿔부를 빠뜨린 초기 분류이므로 결과 판정에 사용하지 않는다. 보정본 `classified_progress.csv`, `classified_status.json`, `classified/`를 사용한다. 원본은 추적성을 위해 보존한다.**',
 '', '## 단계 결과', '', '| 단계 | 결과 | 물리 경과(s) | 실제 경과(s) | 종료 위치 파일 |','|---|---|---:|---:|---|']
 for s in status['stages']:lines.append(f"| {s['stage']} | {s['result']} | {s['stage_sim_s']:.6f} | {s['stage_wall_s']:.3f} | {s['snapshot']} |")
 if not status['stages']:lines.append('| 아직 완료된 단계 없음 | 실행 중 | — | — | — |')
 lines+=['','각 이송의 완료 기준은 대상 전량 도착 후 0.5물리초 유지이다. 최소 10물리초 실행 후 10물리초 동안 목적지 최고 개수가 10개 이상 증가하지 않으면 정체 실패로 기록하고, 실제 도착한 입자로 다음 단계를 검사한다. 최종 혼합은 15RPM·12물리초, 종별 99% 이상 혼합기 도착 및 조성 분산 ≤0.025를 마지막 4초 유지해야 통과한다. 한 단계라도 실패하면 전체 공정 완료가 아니다.',
 '', '## 압력과 밸브', '', '압력 단위 Pa(g), 대기압 0Pa(g)=101325Pa(abs). 절대압=101325+게이지압. 시험 모델 값이며 실제 설비 안전치가 아니다. 공급 A 충전/A만 개방 및 B 충전/B만 개방: +500/-500/0. 칭량→수취: 공급 닫기·칭량 열기·수취 닫기, 0/+500/-1000. 수취→혼합: 공급·칭량 닫기·수취 열기, 0/0/0. 혼합: 모든 출구 닫기, 0/0/0.',
 '', '설정과 적용값, 밸브 요청/실제, 유량은 아래 런타임 CSV에 있다. 닫힘은 입자 점유에 따라 지연될 수 있다. 도착 영역의 좌표 판정과 메시 내부 판정을 동일시하지 않는다.', '', f"런타임 로그: `{status.get('run_log','준비 중')}`"]
 a=status.get('latest',{}).get('actual_runtime_row',{})
 if a:
  lines+=['',f"마지막 런타임 기록: t={a.get('sim_s')}s, 준비={a.get('setup_s')}s, 활성 계산={a.get('active_wall_s')}s, 준비 후 실제={a.get('elapsed_since_ready_s')}s.",
   f"압력 설정 S/W/R={a.get('supply_set_Pa_g')}/{a.get('WH_set_Pa_g')}/{a.get('RH_set_Pa_g')}; 적용={a.get('supply_applied_Pa_g')}/{a.get('WH_applied_Pa_g')}/{a.get('RH_applied_Pa_g')} Pa(g).",
   f"공급 마스크 요청/적용={a.get('supplier_requested_mask')}/{a.get('supplier_applied_mask')}, 칭량={a.get('WH_requested_open')}/{a.get('WH_applied_open')}, 수취={a.get('RH_requested_open')}/{a.get('RH_applied_open')}, RPM={a.get('mixer_RPM')}."]
 if status.get('error'):lines+=['','## 실행 오류','',status['error']]
 if status.get('protocol_changes'):
  lines+=['','## 명시적으로 변경된 시험 절차','', '전체 경로 검사 요청을 이행하기 위한 시험 절차 조정으로 B 잔류 1개는 장기 정체 기준 미충족으로 기록하고, 실제 도착한 19999개로 후속 경로를 검사한다. 칭량→수취 단계에서도 잔류261개 및 상류1개를 보존하고 실제 수취 도착19738개로 후속 검사한다. 수취→혼합 단계 역시 말기 소량 유입을 미완료로 기록하고 실제 도착19002개(A9396/B9606)로 혼합을 검사한다. 세 단계는 위 기본 10초 정체 기준의 예외이며 몰래 완료 처리한 것이 아니다. ID19261을 포함한 총 20000개는 계속 계산한다. 이후 단계는 해당 단계 시작에 실제 도착한 입자 ID 집합을 별도 파일로 보존하고 그 집합의 도착을 검사한다. 전체 공정 성공 판정과 부분 도착분 검사는 다르다.']
  for change in status['protocol_changes']:lines.append(f"전환 시각 UTC {change['utc']}, 물리 시간 {change['sim_s']:.6f}s, PID {change['pid']}, 초기화 {change['reset']}, 물리 조건 변경 {change['physics_parameters_changed']}. 단계 {change['from_stage']}→{change['to_stage']}, 대상 {change['expected_cohort_count']}개. 자세한 설정/밸브 요청은 protocol_change*.json 참조.")
 lines+=['','## 시간·누락 해석','', '위치와 적용 상태는 비동기 스냅샷이므로 서로 약간 다른 물리 시점일 수 있다. 도착 시각은 첫 관측 시각이며 직전 기록과 해당 기록 사이에 실제 통과가 일어났다. 각 입자의 ID/종/xyz/물리 시각을 원본 및 분류 CSV에 보존한다. 최초 도착/전체 도착 여부는 기록 해상도에 맞춰 해석한다. 중간 중단·초기화 없이 실행 중이며, 실패/종료 사유는 status.json에 기록한다. 준비 시간은 계산 시간과 분리한다.']
 if 'finished_utc' in status:
  lines+=['',f"전체 판정: {'통과' if status.get('completed') else '실패/미완료 — 단계 결과 참조'}",f"최종 혼합 관측: {status.get('latest',{}).get('composition_variance')}"]
 tmp=run/'RESULT.tmp';tmp.write_text('\n'.join(lines)+'\n',encoding='utf8');tmp.replace(run/'RESULT.md')
 return status
while True:
 try:s=write_report()
 except (FileNotFoundError,json.JSONDecodeError,PermissionError):s={}
 if not watch:break
 if 'finished_utc' in s:
  time.sleep(5)
  if s.get('run_log'):shutil.copy2(s['run_log'],run/'runtime_pressure_valves.csv')
  write_report();break
 time.sleep(5)
