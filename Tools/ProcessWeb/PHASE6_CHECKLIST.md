## 2026-11-17 실제 실행 기록

보고서: `Saved/ProcessWebGPUIntegration_20261117_141411/validation_report.txt`. 아래 과거 체크리스트는 모든 변형을 실제 GPU에서 수행했다는 뜻이 아닙니다. 실제 실행과 CPU/모의만 수행한 항목은 보고서에서 구분합니다.

- 실제 GPU: 웹 v1 불러오기 → 별도 v2 저장/재로드/적용, A/B/C 배치 시작, B 부분 수취 중 정지/수동 계산 재개/명시적 남은 양 재개, C 이송 중 −0.5→−1 kPa(g), 예약 C 투입/혼합 종료/RPM0/웹 완료.
- 실제 GPU: 압력 요청 59 ms 뒤 전체 정지로 미전달 요청 취소, 새로고침·오프라인·재접속·제어권 반환/재획득, 계산만 재개 후 미복원.
- 실제 GPU: 별도 예외 배치에서 B 오원료 실제 유입(ID/공급원 확인), 웹 차단 이유·재개 비활성·후속 명령0.
- CPU/모의: 잘못된 값/오래된 세대/동일 UUID 대소문자/중복/다양한 경계 시점 회귀, 마지막 완료 보조 문구 정리와 두 해상도 모의 재갱신/새로고침.
- PIE 재시작/최종 종료 상태는 보고서의 최종 실행 기록을 확인하세요.

# 6단계로 이관한 실제 GPU 통합 검증

**1~5단계 당시에는 실제 GPU 미검증이었으며, 현재 결과는 위 6단계 보고서를 참조**

아래는 이번에 실행하지 않은 항목이다. 3단계 수동 명령 구현을 포함해 4단계 레시피·자동 배치, 5단계 실시간 수취 압력 연결 구현이 끝난 후 함께 수행한다. 앱 포커스 변경이 필요하면 사용자 승인을 먼저 받는다.

- [ ] UI_v2_apply와 기존 로컬·스테이징 상태, 읽기 전용 원본 포인터를 기록한다.
- [ ] 최종 코드로 에디터를 빌드한 뒤 실제 레벨·정확한 Actor와 `-ProcessWebControl` 연결 (조회 전용 `-ProcessWebState` 회귀 포함)을 확인한다. 기존 검증 레시피·장입 데이터와 해시를 사용한다.
- [ ] 원래 작업자 확정 스냅샷의 실행 ID·세대·순서·해석 시각을 웹 원본 JSON과 대조한다. 화면 캡처 표시 시각과 수치 대조 시각을 별도로 기록한다.
- [ ] 공급 A–F 요청/적용 밸브, WH101/RH-201 상태, 모델 질량·입자 수, 호퍼 요청/적용 압력, 유량, 목표/계산 RPM을 같은 시점으로 비교한다.
- [ ] 공급 밸브 열림 중 모델 추정 압력과 구간 차압을 대조한다. 실제 닫힘 후 차압이 지워지는지, 하류 공기만 작동할 때 혼동되지 않는지 확인한다.
- [ ] 웹 조회·새로고침·여러 탭·장비 선택·느린 브라우저가 제어 명령을 발생시키거나 해석 작업자를 대기시키지 않는지 실제 명령 이력과 해석 시간으로 확인한다.
- [ ] 계산 일시정지 동안 통신 하트비트는 유지되는지 확인한다. 에디터 백그라운드 절약·낮은 게임 스레드 주기에서 발행 지연과 GPU 시간 진행을 구분한다.
- [ ] 실제 중계 종료/재시작, 네트워크 요청 중 끊김, 브라우저 재접속을 확인한다. 오래된 값 경고 및 새 수명 데이터 교체를 확인한다.
- [ ] PIE 종료·재시작, 초기화, Actor 교체/연결 해제, 여러 대상 인스턴스에서 이전 실행이 섞이지 않는지 확인한다. 최근 발행자 하트비트가 겹치면 대기하는 상태도 기록한다.
- [ ] A·B 순차 계량과 C 예약 투입에서 원료별 결과와 배치 완료, RPM0이 웹에 정확히 표시되는지 확인한다. 완료 이후 과거 단계 진단이 현재 사유로 남지 않아야 한다.
- [ ] 후속 명령의 요청 ID·세대·제어권 fencing, 중복/이전 요청 거부, 웹/언리얼 동시 조작 차단, 양쪽 전체 정지를 검증한다.
- [ ] 후속 레시피 적용·배치 시작과 실시간 수취 압력 변경을 기존 서비스 경로로 검증한다. 레시피 기준값과 현재 이송 조절값, 이전값→대기→작업자 적용값을 구분한다.
- [ ] 같은 입자 목록·질량 보존·잔류0·계량 공차·게이트 보호·오원료 재개 차단·전체 정지 조건을 그대로 확인한다.
- [ ] 실제 운전 화면 캡처, 수치 대조표, 최종 전체 정지 상태와 남은 제약을 Saved에 남긴다. 원본 브랜치와 기존 스테이징 보존을 다시 확인한다.

이번 1–3단계의 모의 스크린샷과 CPU 직렬화·HTTP 검사는 위 항목의 실제 GPU 성공 증거로 사용하지 않는다.

## 3단계 수동 제어의 추가 확인

- [ ] 브라우저에서 제어권 획득·반환을 실행하고 양쪽 UI가 동일한 소유자를 표시하는지 확인한다. 일반 운전은 한 화면만 허용하고 양쪽 전체 정지는 유지한다.
- [ ] 전체 정지의 요청 중/확인 중/완료/실패를 실제 작업자 결과와 대조한다. 공기 차단, 모든 압력0, 밸브 실제 닫힘, 적용 RPM0 후 일시정지인지 확인한다.
- [ ] 계산만 재개 후 취소된 자동 배치·예약·수취 압력·대기 개방이 복원되지 않는지 확인한다.
- [ ] 각 지원 밸브 개폐와 점유 중 닫힘 보호 대기, 공급 공통/칭량/수취 압력 적용·유량 변화, A/B/C 연결부 공기와 압력을 확인한다. D~F 공기 미지원 표시를 확인한다.
- [ ] 목표 RPM 설정과 혼합 시작을 각각 수행해 실제 GPU 적용 RPM 및 3D 날개 회전을 확인한다. 물리 모델·공차를 변경하지 않는다.
- [ ] 압력 적용 중 웹/언리얼 전체 정지, 네트워크 응답 유실, 세션 만료와 재접속을 확인한다. ID 결과 조회만 수행되고 이전 요청은 재실행되지 않아야 한다.
- [ ] 자동 배치 중 웹 일반 수동 조작이 거부되는지 확인한다. 5단계 전용 수취 압력 제어와 혼동하지 않는다.
- [ ] 실제 에디터 백그라운드 갱신/일시정지 때 lease 만료, 연결 대기, 요청 결과 미확인을 확인한다. 모의 시험 지연을 실제 운전 지연으로 보고하지 않는다.

## Stage 4: recipe / automatic batch / recovery integration

Do not execute these checks in stage 4. **1~5단계 당시에는 실제 GPU 미검증이었으며, 현재 결과는 위 6단계 보고서를 참조**

- [ ] Windows Node + real native link. Record target Actor, map, web/native run/epoch/sequence, current owner. Ask before app focus changes.
- [ ] Same Saved/BatchRecipes list in native and browser; v1/v2, precise target/tolerance, C OFF vs ON0, N/RPM/time/phase pressure restore. Load/save/apply must not advance command revision or move valves/mixer.
- [ ] Explicit recipe apply then one browser start. Capture web RequestId → applied configuration revision/fingerprint → new batch ID/locked JSON. Repeated UUID/refresh never duplicates start.
- [ ] A/B/C GPU initial stocks and immutable source identities. For each material record target, tolerance, actual finalized kg, cohort IDs, receiver and mixer final IDs/masses. Verify union(finalized cohorts) == final mixer cohort; duplicates/missing/unexpected inflow/residual each 0 under existing criteria.
- [ ] Real A→B transfer sequence, C reservation due/open/request/applied times, preserved mixing RPM/time; receiver complete display receipt and subsequent outlet close. Web `배치 완료` only after configured time and this stop's RPM 0 acknowledgement.
- [ ] Partial arrival stop: pre-request and confirmed-stop snapshots/IDs/masses; all air off, pressure 0, valves physically closed, RPM 0, worker paused. Manual action, calculation-only resume (no automatic reissue), stop again, explicit batch check.
- [ ] Before resume compare already passed / in connector / in transfer / receiver / mixer against immutable original records. Additional supply excludes all material already downstream; finalized cohorts never recapture old arrivals. Capture movement summary and check token/revisions.
- [ ] Explicit remaining-only resume, repeated same UUID, worker approval and retained batch/transfer/cohort IDs. Record newly arrived mass separately and compare to remaining amount, then batch complete.
- [ ] Actual wrong-source particles after manual action: record source+particle ID+region, sticky contamination reason on web, disabled resume and zero follow-on automatic commands. Do not substitute mock evidence.
- [ ] Check invalidation after manual request, motion, reset/epoch or target change. Expired C reservation must remain blocked, no late auto-injection.
- [ ] Disconnect/refresh during active batch: native continues the same batch with locked settings; no automatic authority reacquire/replay. Reconnect sees same applied config, execution config, batch and request results. Separate editor/PIE restart/reset verifies old-run rejection.
- [ ] Completion/abandon preserves results and shows real next-start prerequisites. Finally whole stop and verify all physical stop acknowledgements. Source branch pointer/staging/recipes preservation audit.


## 5단계 전용 압력 경로 — 실제 GPU에서 수행할 항목

이번 단계에서는 아래 시험 및 실제 운전 캡처를 실행하지 않았다.

- [ ] 기존 장입 데이터·발표용 레시피를 웹에서 불러와 적용한 뒤 시작을 한 번 요청한다. 수취 −0.5 kPa(g), 공급 0 / 칭량 +0.5의 실제 적용값을 확인한다.
- [ ] 실제 칭량 출구 열림·수취 출구 닫힘·배관 이동 중인 입자를 확인한 뒤 **웹 수취호퍼 전용 입력/적용 버튼**으로 −1.0 kPa(g)를 요청한다. 요청/접수/작업자 적용의 해석·벽시계 시각과 UUID/native revision을 기록한다.
- [ ] 동일 실행/세대/배치/이송 ID, 확정 기준 입자/질량을 유지하는지 확인한다. 같은 확정 스냅샷의 압력·계산 유량과 동일 입자 ID의 전후 위치/진행을 기록한다. 공급·칭량 압력과 다른 밸브 상태는 유지돼야 한다. 적용 대기 중 해석 시간이 진행되는지 확인한다.
- [ ] 기존 유입/잔류0·계량/이송 공차로 수취 도착 → 혼합기 전량 투입 → 출구 실제 닫힘 → 혼합 종료 → 이번 정지의 적용 RPM0 → 웹 `배치 완료`를 확인한다. 원료별 확정/도착 질량과 ID 합집합을 대조해 중복·누락·추가 유입·경로 잔류를 기록한다.
- [ ] 다음 이송은 저장 레시피 압력으로 시작하고 이전 조절값이나 요청이 복원되지 않는지 확인한다. 잘못된 단계/열린 수취 출구/범위 밖 입력은 이유가 표시되고 요청되지 않아야 한다.
- [ ] 별도 초기화 실행에서 **압력 요청 직후 전체 정지**: 공기 차단·압력0·실제 밸브 닫힘·RPM0·작업자 일시정지와 미확인 장비 표시를 대조한다. 재접속/제어권 획득/계산만 재개 후 취소된 요청이 복원되지 않아야 한다. 명시적 배치 재개도 원래 복구 절차를 사용해야 한다.
- [ ] 이송 종료/다음 이송 전환과 요청 도착이 겹치는 경우, 제어권 만료/응답 유실 후 결과 조회, 실제 PIE 재시작·초기화를 검증한다. 서버 접수/작업자 적용/결과 미확인을 구분한다.
- [ ] 변경 전·적용 후 웹 화면, 실제 3D 이동, 배치 완료 캡처를 Saved에 남긴다. 화면 시각과 수치용 확정 스냅샷 시각을 구분한다. 앱 포커스 변경은 사전 승인 후 수행한다.

**1~5단계 당시에는 실제 GPU 미검증이었으며, 현재 결과는 위 6단계 보고서를 참조**. 모의 유량 변화·모의 완료 화면은 실제 성능/전량 이송의 증거가 아니다.

## 4번 작업 후 MX-301 실시간 RPM 통합 검증

구현 상태이며 아래 항목은 모두 미검증이다.

- [ ] 레시피 15 RPM으로 실제 회전을 시작한다. 웹 혼합기 카드에서 7 RPM을 적용한다. 접수·GPU 적용 해석 시각·요청 번호를 기록하고, 같은 해석 시간 동안 GPU 각도 변화량과 언리얼 날개 회전을 비교한다. 1해석초당 기대 회전각은 15 RPM에서 90°, 7 RPM에서 42°다. 웹 제어권 반환 뒤 언리얼 2D에서 15 RPM으로 변경하고 양쪽 표시를 대조한다.
- [ ] 혼합 6해석초, 예약 4초인 시험에서 최초 회전 확인 후 2초에 속도를 바꾼다. 최초 시작 시각, 예약·실제 첨가제 개방 시각, 혼합 경과·잔여 시간, 전량 도착, 이번 종료 명령의 RPM 0과 배치 완료를 대조한다.
- [ ] 첨가제 칭량→수취 이송 중 압력과 혼합 RPM을 각각 바꾼다. 같은 이송 ID·입자 집합과 같은 혼합 운전에서 두 응답 번호, 적용 압력·유량·RPM 및 해석 시간을 함께 기록한다.
- [ ] RPM 요청 직후 전체 정지와 혼합 시간 만료 경계에서 요청을 각각 시험한다. 늦은 요청·재전송·웹 재접속·계산만 재개로 날개가 다시 돌지 않는지 확인한다. 명시적 배치 재개 전 표시한 레시피 RPM, 새 혼합 운전 번호, 보존한 혼합 시간과 남은 시간을 확인한다. 다음 배치도 레시피 RPM에서 시작해야 한다.
- [ ] 0·음수·30 초과·유효하지 않은 수치와 이전 세대·이전 배치·이전 혼합 운전 요청을 거부하는지, 입력 도중 스냅샷이 갱신돼도 초안이 유지되는지 양쪽 UI에서 확인한다. 빌드·자동 시험·모의 브라우저·실제 GPU PIE·화면 캡처 결과를 각각 기록한다.
