# 로컬 상태 연결 규격 v1

**실제 GPU 연결·동작 미검증 — 6단계에서 통합 검증 예정**

## 현재 구현과 선택 근거

엔진 설치 소스의 `Runtime/Online/HTTP`와 `Runtime/Online/WebSockets` 모듈을 확인했다. 현재 Windows 엔진은 비동기 HTTP 요청과 WebSocket 클라이언트를 지원한다. 이번에는 **엔진 HTTP 발행 + Node.js 루프백 중계 + 브라우저 HTTP 조회**를 선택했다. 1–2단계의 전송 방식을 유지해 3단계에도 WebSocket 재전송 계층을 추가하지 않고, 엔진 기본 HTTP 모듈과 Node 기본 라이브러리만 사용한다. MCP/Remote Control의 객체 쓰기·콘솔 API를 웹에 노출하지 않는다.

```text
기존 작업자 확정 스냅샷
  → 기존 ProcessControlService.State.Snapshot
  → 게임 스레드의 일관된 복사·직렬화
  → POST http://127.0.0.1:8766/v1/state
  → 로컬 중계 서버의 최신 메시지 한 건
  → GET http://localhost:8765/api/state
  → 상태 공정도 + 명시적 수동 조작
```

`POST /v1/state` 응답에 인증된 제어권 프레임과 미전달 명령이 포함됩니다. `-ProcessWebControl` 옵션의 게임 스레드 콜백만 이를 처리하며 기존 `-ProcessWebState`는 응답을 실행하지 않습니다. 브라우저의 일반 GET은 명령을 만들지 않습니다. UObject/콘솔/임의 함수 호출 API는 없습니다.

## 명시적 대상

- 레벨: `/Game/ProcessPlant_20261111_LowPoly/L_ProcessPlant_LowPoly_1111`
- 정규 Actor 경로: `/Game/ProcessPlant_20261111_LowPoly/L_ProcessPlant_LowPoly_1111.L_ProcessPlant_LowPoly_1111:PersistentLevel.PowderPlantTest_0`
- `ProcessPlantBinding::SourcePath`를 재사용하고 PIE 인스턴스 경로를 보정한다.
- 기존 UI 서브시스템의 `ControlService.GetSource()`와 정확한 객체 참조·월드·스냅샷 Actor 경로가 모두 일치해야 한다. 이름 검색의 첫 번째 Actor를 선택하지 않는다.
- 해당 레벨의 실행 월드가 하나가 아니거나 UI 연결이 없으면 사유·다음 행동이 있는 `waiting`, `snapshot:null`을 보낸다.
- 각 실행 월드는 독립 발행자 ID를 사용한다. 최근 3초 안에 여러 발행자의 하트비트가 있으면 중계는 `ambiguous`로 표시하고 스냅샷을 숨긴다. PIE 재시작 직후에는 이전 발행자의 만료까지 잠시 이 상태일 수 있다.

## 엔드포인트와 수명

| 경로 | 허용 동작 | 비고 |
|---|---|---|
| 웹 포트 `/`, `/app.mjs`, `/styles.css`, `/protocol.mjs`, `/commands.mjs`, `/manual.mjs` | GET | 고정된 정적 파일 목록만 제공 |
| 웹 포트 `/api/state` | GET | 중계 상태 + 최신 전체 스냅샷 |
| 발행 포트 `/v1/state` | POST | 별도 Bearer 인증, 상태 수신 + 제어 프레임 응답 |
| 웹 `/api/session`, `/api/heartbeat`, `/api/acquire`, `/api/release` | POST | 동일 Origin JSON, 세션·제어권 관리 |
| 웹 `/api/command`, `/api/result` | POST | 세션 인증, 허용 명령 접수 / 요청 ID 결과 조회 |
| 기타 경로 | 거부 | 404/405, 임의 UObject 호출 없음 |

두 포트는 IPv4 `127.0.0.1`에만 바인딩한다. Host/Origin 검사, CORS 미제공, 캐시 금지, CSP를 적용한다. 발행 인증값은 중계 시작 때 생성해 `Saved/ProcessWeb/link.json`에만 기록한다. 네트워크 URL은 항상 코드가 조합하는 루프백 주소이며 파일에서 외부 URL을 읽지 않는다. 이것은 동일 PC 내 실수·다른 웹 출처의 접근을 제한하는 범위이며, 같은 사용자 권한으로 파일을 읽는 프로그램과의 보안 격리는 아니다.

- 발행 목표 주기: 게임 스레드에서 **250 ms**. 하트비트는 해석 시간이 같아도 발행한다.
- 브라우저: 응답 완료 후 **250 ms** 뒤 다음 GET. 한 탭에 한 요청만 진행한다.
- 엔진: 진행 중 HTTP 요청 최대 1개, **2초** 제한, 대기 중 새 표본은 누적하지 않는다. 네트워크 응답을 기다리며 작업자를 멈추지 않는다.
- 2초를 넘은 확정 스냅샷은 `stale`. 중계가 받은 후의 경과 시간도 포함한다.
- 발행 하트비트가 3초 동안 없으면 `disconnected`. 이전 값을 유지할 경우 `data_stale:true`와 경고를 표시한다.
- 브라우저 응답 제한: 1.8초. 실패 후 재조회하며 이전 값은 오래된 값으로 표시한다.
- 상태 메시지 상한: 512 KiB (명령 결과 포함), 브라우저 요청 16 KiB, 엔진 응답 32 KiB, 장비 배열 64개. 엔진의 현행 공급 매핑은 A–F이며 웹 레이아웃은 데이터의 장비 개수에 따라 늘어난다.
- 서버는 스냅샷 최신값만 보유한다. 수동 요청 결과 원장은 아래 규칙에 따라 별도로 보관한다. 느린 연결은 제한 시간 후 닫으며 웹 연결 32개·발행 연결 8개로 제한한다. 만료된 발행자 기록을 정리한다.
- 월드 종료 시 엔진 콜백을 해제하고 HTTP 요청을 취소한다. Node 종료 시 연결·타이머·자신이 생성한 연결 파일을 정리한다. 탭 종료 시 fetch/timer를 취소한다.
- 조회 탭의 종료·새로고침·다중 탭 조회는 장비 명령을 만들지 않는다. 제어권을 가진 탭이 종료되면 미전달 명령을 취소하며, 이미 서비스에 전달된 적용 대기 명령은 기존 전체 정지로 취소·확인한다.

게임 스레드가 백그라운드 절약이나 에디터 일시정지로 느려지면 발행 주기도 느려질 수 있다. 이 경우 웹은 갱신 지연을 숨기지 않는다. 작업자의 GPU 계산과 게임 스레드 발행 지연은 6단계에서 따로 대조한다.

## 발행 메시지

| 필드 | 형식·의미 |
|---|---|
| `schema_version` | 숫자 1. 다른 버전 거부 |
| `publisher_id` | 에디터 프로세스 UUID + 월드/연결 실행 UUID. 중계 발행자 구분 |
| `run_id` | PIE/게임 시작, 명시적 Actor 교체 및 서비스 재연결 시 새 UUID |
| `publication_sequence` | 64비트 정수의 십진 문자열. 같은 프로세스의 발행 순서 |
| `level`, `actor` | 위 정규 대상. 다르면 거부 |
| `binding` | `status`, `reason`, `next_action`. 미연결 시 `snapshot:null` |
| `control` | `owner:unreal/web`, `web_read_only`, `transfer_supported`, `command_version:1`, `lease_id`, `acquire_reason`, `actions`, `results`, `pause_policy`, `unconfirmed` |
| `snapshot.epoch`, `.sequence` | 초기화 세대·작업자 순서. 64비트 정밀도 보존을 위해 문자열 |
| `.simulation_seconds` | 해석 시간, 초. 미확정 상태는 null, 벽시계 시간이 아님 |
| `.age_ms`, `.updated_utc` | 확정 데이터 경과 시간과 진단용 UTC |
| `.validity`, `.execution`, `.error` | 데이터 유효성과 계산 실행 상태를 분리 |
| `.actual_actor_path` | PIE 접두사를 포함한 실제 Actor 경로 |
| `.controller_observed_sequence` | 자동 상태와 함께 복사한 서비스 스냅샷 순서 |
| `.control_revisions` | 요청·작업자 처리·압력 적용·RPM 적용 revision, 십진 문자열 |
| `.equipment`, `.regions`, `.batch` | 아래 상태 항목 |

모든 장비 수치는 `{value, unit, validity, reason, source}` 형식이다. `valid`일 때만 유한한 수치가 있고, 나머지는 `value:null`이다. 유효성은 `valid`, `unknown`, `initializing`, `stale`, `source_missing`, `unsupported`, `error`, `not_computed`를 구분한다. 일반 어댑터의 판정을 재사용하며 초기 장입량을 현재 재고로 대신 표시하지 않는다.

순서는 발행자별로 검사한다. 발행 순서 역전·중복, 같은 실행의 세대 역전, 같은 세대의 스냅샷 순서 역전, 폐기한 이전 실행 ID를 거부한다. 동일 작업자 순서의 **새 하트비트**는 허용한다. 초기화는 이전 구역·원료 결과를 부분 병합하지 않고 메시지 전체를 교체한다. 브라우저도 중계 실행 ID와 응답 순서를 검사해 이전 중계의 늦은 응답을 거부한다.

## 데이터 출처와 단위

| 항목 | 원본·의미 | 전달 단위 / 표시 |
|---|---|---|
| 밸브 요청·적용 | `BuildEquipment`의 `ValveRequestedOpen` / `ValveAppliedOpen` | bool 0/1 / 열림·닫힘, 미확인은 null |
| 초기 장입 입자 | `InitialSupplierCounts` | particles / 개, 초기량으로 명시 |
| 공급 현재 재고 | 유효한 `Recovery.Materials`의 공급원별 `Source` | particles, 해당 개수 × `ModelParticleMassKg` kg |
| 칭량·수취 현재량 | `WeighingMass`, `ReceiverMass`, `InWeighingHopper`, `InReceiver` | kg·particles / g 또는 개 |
| 혼합기 현재량 | `InMixer`, 확인된 모델 입자 질량 | kg·particles / g 또는 개 |
| 칭량–수취 배관 | `PipeMass`; 균일 입자 질량으로 입자 수 환산 | kg·particles, 초기 원료/배치 도착량과 구분 |
| 수취–혼합 연결부 | 유효한 `MixerFeed.ConnectorKg` | kg, 이번 확정 배치의 잔량으로 명시 |
| 호퍼 요청·적용 압력 | `RequestedGaugePressurePa` / `AppliedGaugePressurePa`와 `AppliedPressureValid` | Pa(g) / kPa(g), ÷1000 |
| 공급 밸브 직전·직후 | 작업자의 `SupplyValvePressures` | Pa(g) / kPa(g), 모델 추정값 |
| 밸브 앞뒤 구간 차압 | 같은 스냅샷 `DifferencePa` | Pa / kPa, 직전−직후 |
| 공급 경로 유량 | 같은 밸브 압력 스냅샷 `FlowM3s` | m3/s / m³/s |
| 칭량–수취 유량 | `Transfer.CalculatedCarrierFlowM3s`, `CarrierFlowValid` | m3/s / m³/s |
| 하류 독립 공기 | A/B/C `Supply*Purge.FlowM3s` | m3/s, 공급 밸브 통과 유량과 별도 |
| 목표·계산 RPM | `RequestedRPM`, `AppliedRPM`와 유효성 | rpm / RPM |
| 배치 단계 | 같은 게임 스레드에서 복사한 `AutoRun.State`, 세대 일치 검사 | 단계 ID·기존 한국어 단계명·이유 |
| 정지·차단 | 기존 `StopDescription`, 현재 취소/실패의 복구 사유, 시작 전 `CanStart` | 기존 사유 그대로. 완료에 과거 복구 진단을 붙이지 않음 |
| 원료별 결과 | 작업자의 `SequentialBatch.A/B/Additive` 확정량·현재 도착 입자 | kg, 미확정 결과는 null |

호퍼 압력은 **적용 경계 설정**이며 센서 측정값/용기 감압 완료를 뜻하지 않는다. 밸브 직전·직후 압력은 이미 계산된 모델 추정값을 전달하며 손실계수·물리 모델을 추가하지 않는다. 밸브가 실제로 닫혔으면 이전 추정값을 null로 지우고 `닫힘 · 차압 미산출`을 표시한다. 적용 상태 자체가 미확인이면 닫힘으로 단정하지 않는다.

숫자 표시만 간결하게 반올림한다. 웹은 레시피나 제어값을 수정하지 않는다. 작은 비영 질량은 `<0.01 g`, 미확인은 `미확인`, 확인된 빈 구역은 `0 g`으로 표시한다. 원시 정밀도는 JSON에 유지된다.

## 브라우저 응답과 화면

`GET /api/state`는 `schema_version`, `server_id`, `response_sequence`, `mock`, `connection`, `data_stale`, `simulation_progress`, `envelope`를 반환한다. 중계 식별자와 응답 순서는 브라우저 응답 순서 검사용이며 작업자 순서와 별개다.

- `connection`: 중계가 확인한 언리얼 하트비트 연결 상태와 이유·다음 행동.
- `simulation_progress`: `paused`, `advancing`, `not_advancing`, `unknown`. 진행 중단만으로 막힘·오류를 추정하지 않는다.
- `mock:true`: 별도 모의 실행에서만 설정. 화면 상단 표시를 숨기지 않는다.
- 무효한 형식은 이전 화면의 일부 필드에 섞지 않는다. 오류를 표시하고 이전 값이 있다면 오래된 값으로 표시한다.

## 3단계 명령 규격 v1

```json
{"command_version":1,"request_id":"00000000-0000-4000-8000-000000000001",
 "run_id":"현재 발행 run_id","epoch":"1","lease_id":"현재 제어권 UUID",
 "equipment":"receiver","kind":"pressure","value":-0.5,"unit":"kPa(g)"}
```

| kind | equipment | 값·단위 |
|---|---|---|
| stop_all, resume, pause | plant | 0 / none |
| valve | supply-a~supply-f, weigher, receiver | 0 또는 1 / bool |
| pressure | supply-a~supply-f(공통), weigher, receiver | −1~+1, 수취 −1~0 / kPa(g) |
| purge_pressure | supply-a, supply-b, supply-c | 0~1 / kPa(g) |
| purge_enabled | supply-a, supply-b, supply-c | 0 또는 1 / bool |
| rpm | mixer | 0~30 / rpm |
| mixer_enabled | mixer | 0 또는 1 / bool |

`ProcessWebCommands::Decode` 한 곳에서만 압력을 kPa(g) ×1000 = Pa(g)로 변환합니다. 기존 서비스가 지원 여부·현재 운전 조건·게이트 점유 보호·압력 범위·정지 정책을 최종 판정합니다. 요청 값은 보정하지 않습니다. `actions`는 같은 확정 서비스 상태의 허용 사유를 표시하는 힌트이며, 실제 입력값은 요청 시 다시 검증합니다. 연결부 공기와 호퍼 압력은 별도 명령과 적용값입니다.

HTTP 202는 로컬 접수일 뿐입니다. 결과는 `queued`, `accepted`, `processing`, `applied`, `rejected`, `failed`, `unknown`입니다. `applied`는 기존 서비스의 실제 작업자 확인 결과를 복사합니다. 각 결과는 웹 UUID, 기존 서비스 `revision`·`connection`·`epoch`와 결과를 관찰한 스냅샷 순서·해석 시각을 포함합니다. 이 관찰 시각을 실제 물리 적용 순간으로 단정하지 않습니다.

동일 요청 ID와 동일 실행·세대·대상·종류·수치·단위는 저장된 결과만 반환합니다. 내용 또는 브라우저 복원 키가 다르면 거부합니다. native 서비스 History도 같은 UUID를 사용하므로 재전달돼도 장비에 중복 발행하지 않습니다. 서버는 응답에 한 번 실은 명령을 자동 재전달하지 않습니다. 응답이 유실되면 다음 상태의 결과로 대조하거나 `unknown`으로 남습니다. 브라우저는 결과만 재조회합니다.

일반 명령 원장은 서버 수명당 240개로 제한합니다. 전체 정지는 별도 우선 경로이며 보관된 실행 결과가 256개에 도달하면 오래된 종료 항목을 재실행 불가능한 결과 영수증으로 압축합니다. UUID·내용의 영수증은 해당 서버 수명 동안 유지합니다. 서버 재시작 후 이전 요청은 미확인으로 남으며 자동 재전송하지 않습니다. 실행·초기화 변경은 제어권과 미전달 요청을 무효화합니다.

## 제어권과 보안

- 웹 세션은 동일 Origin의 JSON POST로 발급합니다. 예측 불가능한 세션 토큰은 헤더에만 사용합니다. GET, 다른 Origin, Origin 없음, 잘못된 Content-Type, cross-site Fetch Metadata, 인증 없는 조작은 거부합니다. CORS/cookie/query-token은 사용하지 않습니다.
- 브라우저 탭의 복원 키·요청 목록은 sessionStorage에만 둡니다. 새로고침의 세션 재발급은 같은 탭의 기존 제어권을 해제합니다. 다른 탭은 새 키를 사용합니다.
- 획득은 새 UUID lease의 명시적 요청입니다. 서버는 한 세션만 선택하고 언리얼 서비스가 같은 lease를 상태에 확인해야 웹 조작이 활성화됩니다. 대상 run/epoch, 서버 수명, lease UUID와 만료 시간이 fencing 역할을 합니다. native 이전 HTTP 응답은 서버·실행 식별자로 거부합니다.
- 브라우저 하트비트 0.75초, lease 3초. 엔진도 벽시계 만료를 독립 확인하며 HTTP 왕복 시간을 남은 유효기간에서 차감합니다. 게임 스레드가 멈춘 동안 쌓인 요청을 재개 때 자동 적용하지 않습니다.
- 자동 배치 또는 기존 로컬 요청 처리 중에는 제어권을 넘기지 않습니다. 웹 소유 중 언리얼 일반 Request/자동 시작/배치 재개/자동 수취 압력 전용 진입점은 서비스에서 차단하고 UI도 소유자를 표시합니다. 전체 정지는 양쪽에서 가능합니다.
- 세션/lease 종료는 미전달 일반 명령을 거부 처리합니다. 이미 전달된 웹 명령이 서비스에서 적용 대기 중이면 기존 전체 정지 절차를 호출해 대기 개방·공기 토큰을 취소합니다. 이미 적용 완료된 설정을 원복하지 않습니다. 자동 배치를 재시작하지 않습니다.
- 전체 정지가 접수되면 큐의 기존 일반 명령을 취소합니다. native 프레임에서도 정지를 먼저 처리하고 같은 프레임의 일반 요청은 거부합니다. 처리 중인 장비 요청은 기존 StopAll 상태기의 무효화·압력0·공기차단·게이트보호·RPM0·일시정지/실패동결 경로를 그대로 사용합니다.

## 후속 단계

4단계 레시피·자동 배치, 5단계 이송 ID에 결합한 실시간 수취 압력 변경은 아직 웹에서 허용하지 않습니다. 자동 배치 중 일반 pressure 명령을 기존 `RequestTransferReceiverPressure` 경로로 우회시키지 않습니다. 실행별 명령/제어권과 상태의 실제 일치는 6단계 GPU 통합 검증에서 확인합니다.

## Stage 4 configuration and batch operations

Transport remains authenticated `/v1/state` ↔ `/api/command`, with exact Origin/session/lease enforcement. No browser-supplied filesystem paths or production Node filesystem access. `FProcessWebBatch` executes configuration/storage on the game thread using `FProcessRecipeStore`, `ApplyRecipe`, `StartAutomaticA`, `CheckAutomaticBatch`, `ResumeAutomaticBatch`, `AbandonAutomaticBatch`. The existing native automatic state machine and worker remain the sole controller. Physics, tolerances, guards and recovery cohorts are unchanged.

Every operation retains command_version=1, UUID request_id, run_id, decimal-string epoch, lease_id, equipment=`plant`, value=0, unit=`none`. The following kinds are additive:

| kind | Additional fields | Result data / meaning |
|---|---|---|
| recipe_list | none | `files[]` with opaque file_id, name, file_version, error; max 128, truncated flag |
| recipe_load | file_id | recipe_json, file_version; fills draft only |
| recipe_save | recipe_json; overwrite additionally file_id, file_version, confirm=true | file_id, new file_version; no device command |
| recipe_apply | recipe_json, recipe_revision, recipe_fingerprint | next-run recipe_revision/fingerprint; full native validation, no device command |
| batch_start | recipe_revision, recipe_fingerprint | generated batch_id, pinned recipe_revision; admission ≠ batch completion |
| batch_check | batch_id | check_token only after current recovery proof |
| batch_resume | batch_id, check_token | batch_id; processing until current service RecoverBatch RequestId, accepted time, connection, epoch and applied revision match worker recovery approval |
| batch_abandon | batch_id, confirm=true | target batch preserved but automatic continuation abandoned; existing whole-stop called |

`recipe_json` is the unchanged v1/v2 JSON string (max 8192 characters at web boundary); read uses native decode and returns canonical v2. kg/Pa values do not roundtrip through display strings if unedited. Units, keys, modes, order, source IDs, names, positive time/RPM and pressures are validated in browser/server for feedback and again natively. Field JSON validation failure does not partially update configuration. No particle identities, measured results, active valves or queued commands belong to a recipe.

Production `file_id` is an opaque UUID mapped to a validated filename by the current native bridge. IDs may change with editor/world restart; refresh the list. `file_version` is MD5 of actual file bytes (conflict token, not authentication). Writers acquire a shared CREATE_NEW/O_EXCL `.recipe-write.lock`, compare the expected version under lock, write a same-directory temp and atomically replace. The native recipe UI also passes the selected version. Uncooperative external editors do not honor the lock; refresh/reload after editing outside the project tools. Crash-left locks are not automatically deleted.

`recipe_revision` + `recipe_fingerprint` identify current applied settings. The fingerprint also catches native individual-setting changes without a revision increment. The service locks execution JSON when a new batch starts; web-start execution revision is retained separately. Native-start batches may have no web revision, but their original execution JSON remains authoritative. Execution ID/epoch/authority and actual start conditions are rechecked; retries with the same request UUID return the stored receipt without starting another batch.

`control.batch_workflow` carries applied_json, recipe_revision/fingerprint, execution_json/revision/fingerprint, batch_id, active_supplier, mixing elapsed/remaining/duration, additive status/due time, transfer id/baseline/remaining/transit/arrived/complete, and recovery pending/checked/check_token/reason/next_stage/materials/movement_summary. Masses are kg; missing proof is null, not 0. Recovery amounts are projected from existing worker `Recovery.Materials`, ModelParticleMassKg and the original locked per-material targets/cohorts. `additional_supply_kg` excludes already passed connector material. `remaining_transfer_kg` is current accepted material not yet in the mixer. Arrival values are present inventory, not cumulatively counted crossings.

A recovery receipt is tied to batch ID, StopRequest, LastRequest, connection, epoch, requested control revision, physical time and worker recovery accepted revision, plus a fresh `CanRecover` result. Duplicate paused snapshots alone do not expire it. Manual commands, motion, reset or contamination invalidate it. Resume checks the receipt and existing recovery path again. Worker approval is distinct from HTTP receipt; cancellation/failure cannot be reported as resumed. Calculation-only resume remains the existing manual command and never calls automatic recovery.

Whole stop wins queued mutations. File reads are observational. Disconnect cancels undelivered commands and returns authority; already accepted automatic starts/resumes continue under the native controller. In-flight **manual** commands retain the previous stop-on-lease-loss behavior. Native recipe editing and individual automatic settings are fenced while web owns control. Authority can be explicitly reacquired during an automatic batch without altering it; mutation capabilities remain restricted.

The existing native sequence waits for a displayed receiver-transfer completion before closing the weighing outlet. Web `heartbeat.observed_transfer` acknowledges a **rendered complete transfer** with run_id/epoch/batch_id/transfer_id/snapshot sequence. The relay checks its current published complete transfer; native verifies the still-active matching complete transfer before calling the existing `ConfirmAutomaticUI`. This is a display receipt, not a new start/open/pressure command. Refresh/read receipts cannot create a batch. Native physical completion conditions still gate progression.

Relay sends one command per native exchange (250 ms nominal publication), keeping responses below the existing 32 KiB limit and prioritizing stops. Native receipt payloads are released only after relay `result_acks`; relay retains result data for browser result lookup. Request limits remain stage 3 limits. Network delay changes admission latency, not worker cutoff physics. Server/engine restart recovery of the request ledger is not implemented; previous ambiguous requests require state inspection, never automatic replay.

CPU/mock records and screenshots are not GPU evidence. Production file sharing, native transport during actual PIE, mass conservation, particle identity and physical batch outcomes are stage 6 work.


## Stage 5 — scoped receiver pressure override

Existing envelope schema/command version 1 remains compatible; `control.transfer_pressure` is an optional new projection. Older publishers without it cannot expose this action. No new HTTP endpoint, worker protocol, physics parameter or recipe version is introduced.

```json
{"command_version":1,"request_id":"<uuid>","run_id":"<current-run>","epoch":"<int64>","lease_id":"<current-web-lease>","equipment":"receiver","kind":"transfer_pressure","value":-1,"unit":"kPa(g)","batch_id":"<current-batch-uuid>","transfer_id":"<positive-int64>"}
```

`value` is finite and in [-1,0]. IDs retain decimal-string integer precision. The server forwards kPa unchanged; **ProcessWebCommands::Decode converts once using PlantControl::PaPerKPa (1000)**. The receiver-only dedicated command calls `RequestWebTransferReceiverPressure` → `RequestTransferReceiverPressure` → original `AutoRun.RequestReceiverPressure`. Ordinary `pressure` remains blocked during automatic batches.

The server validates run/epoch, acknowledged lease, batch/transfer scope, current native capability at admission and again before delivery. One outstanding adjustment per transfer is allowed; a new UUID while pending is rejected even for the same value. Replaying the same UUID returns the original receipt and never dispatches again. Payload fingerprints include batch/transfer IDs. Native admission repeats the lease/epoch/batch/transfer checks **after Tick observes the worker**, so completion, reset or disconnection between display and delivery wins. Original dose, outlet, air, supply/WH pressure and additional-inflow checks remain authoritative. The guard only expands the web adapter, not the allowed physical conditions.

`control.transfer_pressure` is projected from the already observed service state on the game thread. It contains:

| Field | Meaning |
|---|---|
| batch_id, transfer_id, epoch, sequence, simulation_seconds | Scope and exact snapshot cursor; no transfer → `0` ID |
| validity, available, reason, stage | Data health, native admission and blocking reason; web lease/link checks additionally apply |
| recipe_pa | Locked current batch's transfer receiver target, Pa(g) |
| requested_pa | Accepted dedicated adjustment for this transfer, Pa(g), otherwise null |
| applied_pa | Latest confirmed worker receiver pressure setting, Pa(g); null when stale/unknown |
| flow_m3_s | Same snapshot transfer calculated carrier flow, m³/s; null if validity/path not confirmed |
| flow_reason | Why the flow was not computed; unknown is never substituted by zero |
| service_request_id, pending, status | Original native request linkage and scope-limited state |
| requested_s, applied_s | Confirmed simulation times; unknown is null |
| pressure_applied_revision | Worker pressure revision as decimal string |

Recipe target is an immutable setting, not a sensor result. No vessel pressure convergence claim is made. The UI divides Pa by 1000 only for display; requested values cannot replace actual values. Projection epoch/sequence/time must match the containing snapshot; mixed snapshots are rejected by protocol validation.

Results retain the web UUID plus `service_request_id`, batch/transfer scope, native revision and last observation sequence/time. Native result history and `ObserveReceiverPressure` establish application (worker acknowledgement + newer snapshot + matching applied revision/requested/applied pressure). Server transport acceptance alone does not. No valve reopening, recapture, batch/transfer restart or cohort modification is part of this command.

Queued adjustments are rejected if the transfer finishes/changes, control is lost or a stop takes priority. Delivered requests stay result-tracked; an unknown outcome must not be retried with a new UUID automatically. Stop cancels through the original native flow, including the existing pressure revocation and worker freeze policy. In-flight web requests on lease loss invoke the pre-existing bridge stop path; already applied adjustments do not stop an ongoing batch just because an observer disconnects. Reconnect queries existing receipts and requires explicit authority acquisition. Neither calculation-only resume nor batch resume restores a canceled pressure command. The next transfer uses the locked recipe target.

Mock scenario `pressure` reuses the temporary recipe/batch peer, delayed acknowledgements and server security path. Its flow values and automatic progression are prescribed fixtures, never a physics surrogate. No production recipe, model or launch settings are changed.


## 6단계 실제 연결에서 확인한 UUID/결과 처리

브라우저 UUID는 소문자이고 Unreal FGuid는 대문자로 응답할 수 있습니다. 서버는 요청 ID 비교/중복 방지/결과 조회의 키를 소문자로 정규화하고, 외부 영수증의 `request_id`는 최초 브라우저 표기를 유지합니다. 같은 UUID의 대소문자 차이는 새 요청이 아닙니다. 네이티브 `service_request_id` 표기는 별도 보존합니다.

작업자 결과의 `simulation_seconds`는 전달된 최신 스냅샷 시각일 수 있습니다. 물리 적용 시각은 압력의 applied simulation seconds, RPM applied/stopped seconds 등 해당 작업자 이벤트 필드를 사용합니다. HTTP 접수 또는 캡처 시각을 물리 적용 시각으로 사용하지 않습니다.

전체 정지 `unconfirmed`는 정지 진행 중 또는 실패 시에만 현재 미확인 장비를 나타냅니다. 완료 뒤 빈 목록은 작업자 정지 조건 확인 결과이며, 후속 운전까지 지속되는 안전 보증이나 분진 안전 판정이 아닙니다.

실제 GPU 사례·요청 ID·동일 스냅샷 수치 대조: `Saved/ProcessWebGPUIntegration_20261117_141411/validation_report.txt`.

## 자동 구간의 호퍼 압력

`transfer_pressure`의 장비 ID는 공급 중 현재 `supply-a`~`supply-f` 중 하나 또는 `weigher`, 칭량→수취 이송 중에는 `weigher` 또는 `receiver`다. 요청은 기존 실행 ID·세대·제어권·요청 ID에 `batch_id`, `stage_token`, `supplier_id`, `transfer_id`를 포함한다. 공급 단계의 이송 ID는 문자열 `"0"`이며 수취 이송에는 실제 이송 ID를 쓴다. 단계 토큰은 구간에 진입할 때마다 새로 발급한다. 새 작업자 스냅샷을 읽은 뒤에도 같은 단계인지 다시 대조한다.

`control.transfer_pressure.targets[equipment]`가 장비별 허용 여부와 사유, 레시피·접수·작업자 적용 압력(Pa), 현재 유로 계산 유량(m³/s)을 제공한다. 미확인 수치는 null이다. 이전 수취 전용 표시 필드도 남아 있다. 브라우저의 명령 단위는 kPa(g)이며 변환은 언리얼 브리지에서 수행한다. 조회·재접속은 압력 명령을 만들지 않는다. 현재 구간과 다음 구간의 요청은 서로 다른 토큰으로 구분한다.
