# ProcessUIV2 — 실시간 상태 조회 연결

`L_ProcessPlant_FlowTest`에서 Play하면 기존 `WBP_ProcessUIV2`를 코드로 생성합니다. 공유 맵과 위젯 에셋은 수정하지 않습니다. `UProcessUIStateSubsystem`은 다음 **명시적 Actor 참조**만 사용합니다.

`/Game/ProcessPlant_20261029/L_ProcessPlant_FlowTest.L_ProcessPlant_FlowTest:PersistentLevel.PowderPlantTest_0`

PIE에서는 이 참조를 해당 PIE 패키지로 변환합니다. 참조가 없으면 다른 Actor를 탐색하지 않고 `시뮬레이션에 연결되지 않았습니다`를 표시합니다. 다른 인스턴스를 연결할 때는 `OpenForActor(명시적 Actor 참조)`를 호출합니다. 기존 테스트 Host를 추가 소환하면 별도의 미연결 UI가 중복되므로 이 경로에서는 소환하지 않습니다.

## 현재 연결 계약

- 공급원: `APowderPlantTest::GetSimulationSnapshot()` 하나. `UProcessUIStateAdapter`가 조회 요청과 표시 함수 사이를 연결하며 명령을 전달하지 않습니다.
- `OnStateRequested`는 즉시 갱신하고, 선택/호버 장비는 벽시계 기준 약 0.1초마다 갱신합니다. 위젯 파괴·제거·월드 종료 시 바인딩을 정리합니다.
- 각 상태는 장비 ID, 값/단위/유효성, 출처, UTC 발행 시각, 초기화 세대, 스냅샷 순서를 포함합니다. 출처 표시는 `실시간 시뮬레이션 · 미보정`; 출처 툴팁에서 Actor 경로·시각·세대·순서를 확인합니다.
- 작업자가 mutex 아래 발행한 확정 결과만 복사합니다. UI는 GPU 상태를 직접 읽거나 해석을 진행시키지 않습니다. 일시정지 중 상태 응답은 계산을 진행하지 않는 heartbeat입니다. 2초간 새 발행이 없으면 오래된 값으로 표시하며 이전 숫자를 숨깁니다.
- `supply-a/b/c`는 게이트 인덱스 0/1/2입니다. 공급기 현재 잔량은 미집계이며 **초기 적재 수**와 구별합니다. 초기 적재 0개인 C도 구현된 장비입니다.
- `weigher`는 WH101 출구 게이트(ASSUMED_VT)이며 BV122가 아닙니다. `receiver`는 RH201/BV272, `mixer`는 MX-301입니다.
- `supply-d/e/f`는 화면에 남겨 `시뮬레이션 미구현`으로 표시하고 해석 API에 매핑하지 않습니다.
- 칭량호퍼·수취호퍼·혼합기 입자 수는 기존 공간 영역 판정을 그대로 사용합니다. 질량·질량 유량·이송 완료·혼합 품질은 제공하지 않습니다. `ActualRate`도 UI에 이송 유량으로 제공하지 않습니다.
- 요청 RPM은 설정값이며, 적용 RPM은 **마지막 성공한 계산 배치에 사용된 RPM**입니다. 초기 배치 전에는 미확인입니다. 모터 센서값이나 실제 장비 회전수로 표현하지 않습니다.
- 기존 위젯의 미연결 고정 문구는 실행 시 조회 전용 안내로 바꿉니다. D~F는 공정도에서도 미구현으로 표시합니다. 조회 UI가 열려 있는 동안 기존 Slate 수동 패널을 숨겨 3D 화면에서 조작 버튼이 노출되지 않게 합니다.
- 8개 명령 버튼은 계속 비활성이며 조회 화면에서는 숨깁니다. 조회 및 2D/3D UI 전환은 해석 상태를 변경하지 않습니다. 기존 시뮬레이션 자체의 제어 기능은 이 어댑터가 호출하지 않습니다.

## 후속 명령 연결에 유지할 합의

공정은 개별 칭량·이송, 초기 투입과 혼합 후 N초 예약 투입, 예외 시 전체 정지·모든 공정 밸브 닫기, 사용자 조치 후 남은 양 재개입니다. 이 정책은 확정된 합의이며 이번 조회 연결에서 변경하지 않습니다.

현재 작업자는 계산을 일시정지하면 밸브 요청 처리도 멈춥니다. 후속 구현에서 `정지 요청 중 / 밸브 닫힘 대기 / 계산 정지 확인 / 정지 실패`를 구분해야 하며, 요청값을 실제 닫힘으로 표시해서는 안 됩니다. 이번 단계에는 자동 시퀀스, 질량 기반 투입 제어, 정지 정책 변경을 포함하지 않습니다.

## 조회 화면 사용성 개선

- 장비 카드에 밸브의 계산 적용 상태 또는 혼합기의 계산 적용 RPM을 직접 표시합니다. C의 초기 입자 0개와 D~F 미구현을 구분합니다.
- 선택한 카드는 테두리로 강조합니다. 미구현/연결 없음, 준비 중/갱신 중단, 오류를 문구와 배경색으로 함께 구분합니다. 색만으로 상태를 전달하지 않습니다.
- 상세 패널은 장비 설명, 계산 상태/시간, 입자 수, 밸브 또는 RPM, 조회 전용 안내, 출처, 닫기로 정리합니다. 사용할 수 없는 조작 버튼과 입력 폼은 숨기며 명령은 활성화하지 않습니다.
- Actor/작업자 같은 내부 용어를 쉬운 문구로 바꿉니다. 초기 입자 수를 현재 잔량으로 표시하지 않고, 요청 상태와 계산 적용 상태도 계속 구분합니다. 오류 상세와 발행 메타데이터는 툴팁으로 확인합니다.
- ‘계산 중’ 표시가 원료 이송·혼합 실행을 의미하지 않는다는 안내를 공정도에 표시합니다.

공유 맵·위젯 에셋을 저장하지 않고 기존 위젯의 실행 중 표시만 변경합니다. 변경된 문구에 맞춰 기존 위젯 테스트의 기대값을 갱신했습니다. Windows 빌드와 1600×900 별도 게임 창에서 확인했습니다. 카드 줄바꿈과 3D 버튼 글자 넘침을 정리했으며, 출처·조회 전용 안내·장비 상세를 확인했습니다.

## 카메라 입력 보완

조회 연결 초판은 항상 UIOnly 입력 모드를 사용해 기존 카메라 입력까지 차단했습니다. 수정 코드에서는 OnObservationViewRequested를 연결해 2D는 UIOnly, 3D는 GameAndUI로 전환합니다. 기존 카메라의 우클릭 드래그/우클릭+WASD·QE/가운데 버튼/휠을 사용하며 새 카메라 Actor를 선택하거나 생성하지 않습니다. 2D 복귀와 UI 종료 시 남아 있는 이동·캡처 상태를 해제합니다. 조회 UI가 열린 동안 Space/R/O는 기존 해석 제어·초기화·내보내기에 전달하지 않습니다.

3D 관찰에서도 해석 시간과 실행 상태를 계속 표시합니다. 해석 시간 증가와 실제 입자 이송/혼합은 별개입니다. 밸브가 닫혀 있고 혼합이 꺼져 있으면 해석이 실행 중이어도 이송·혼합은 진행하지 않습니다. 이번 보완은 공정 명령을 활성화하지 않습니다.

`ProcessUI.ValidateReadOnlyNavigation` 개발용 검증을 추가했습니다. Slate 입력 경로로 휠, 우클릭+W, 회전을 보내고 2D 전환 시 입력 해제, Space/R 차단, 해석 시간 증가를 확인한 후 프로세스를 종료합니다. 해당 검증은 성공했습니다. 마우스 캡처가 처리된 다음 프레임에 상대 이동을 보내 회전을 검사합니다. 카메라 입력 중 해석 시간은 0.104800초에서 0.115360초로 증가했고 요청 밸브/RPM과 초기화 세대는 유지되었습니다.

## 검증

2026-11-07, Windows UE 5.8 Development Editor 최종 빌드 성공. `ProcessUI.ReadOnly` 자동화 시험 3개 모두 성공했습니다. 카메라 입력 검증(`PROCESS_UI_NAV_PASS`)과 최신 화면 검증(`PROCESS_UI_SCREEN_PASS`)도 성공했습니다.

- `MappingAndFreshness`: ID 매핑, 초기 적재/잔량 구분, 요청/적용 RPM 분리, 오래된 값·Actor 없음·오류, 세대/순서 역전 차단.
- `WidgetBinding`: 실제 Blueprint 장비 선택 변경, Actor 없는 표시, D 미구현/C 구현 구분, 8개 명령 비활성, 2D/3D 선택 유지, 바인딩 해제.
- `WorkerSnapshot`: 실제 OpenCL 작업자 실행/일시정지, 일시정지 heartbeat가 해석 시간을 진행하지 않는지, 요청 15 / 적용 0 rpm, 초기화 세대 증가와 이전 해석 시간 제거.
- 별도 `-game` 창에서 기본 FlowTest와 정확한 Actor 경로 연결 성공. 9개 장비 선택, 2D/3D 전환, 명령 비활성, 조회/전환 전후 요청 상태 불변을 확인했습니다. 캡처에서도 기존 수동 패널 숨김과 조회 전용 문구를 확인했습니다.
- 실화면에서 해석 시간 증가, A/B 초기 적재 각각 10000개, C 초기 0개, D~F 미구현, WH/RH/MX 영역 0개, 밸브 요청/적용 닫힘, 혼합기 요청 15 / 최근 계산 적용 0 rpm을 확인했습니다. 영역 0개는 해당 실행 당시 값이며 다른 시점의 값을 보장하지 않습니다.
- 산출물: `Saved/ProcessUIStateValidation/build.log`, `tests/index.json`, `navigation.log`, `navigation.png`, `screen.log`, `screen_00.png`~`screen_10.png`. 개발용 `ProcessUI.ValidateReadOnlyScreen` 명령은 선택/전환 검증과 캡처 후 **프로세스를 종료**하므로 일반 실행에서는 사용하지 않습니다.

장비 선택/전환 검증은 실제 위젯 함수 호출로 수행했고, 카메라는 Slate 입력 경로로 휠·우클릭+W·상대 마우스 이동을 전달해 검사했습니다. 2D 복귀 시 이동/캡처 해제와 Space/R 공정 제어 차단도 확인했습니다. 최신 코드의 에디터 내부 PIE, 사람이 직접 조작하는 마우스 호버 2.5초·Esc 입력, 가운데 버튼 이동, 작은 해상도, 하드웨어 오류/통신 중단의 실화면 재현은 확인하지 않았습니다. Actor 없음·오류·오래된 값은 자동시험으로 검사했습니다. 사용 가능한 MCP 실행 기능이 제한되어 로컬 빌드/실행 도구로 검증했습니다. `-game` 시작 시 엔진 EditorToolset/ToolsetRegistry Python에서 AgentSkill/PythonTestRunner 속성 오류가 기록되었지만 상태 조회 및 화면 검증은 완료됐습니다.

변경 파일은 `PowderPlantTest.h/.cpp`(확정 결과 조회), `PlantSimulationSnapshot.h`(타입), `ProcessUIStateAdapter.h/.cpp`(조회 연결·화면 생성·사용성 개선·입력 모드 전환), `PowderCameraController.h/.cpp`(카메라 캡처 해제와 2D 입력 보호), `PlantVisibilityController.cpp`(조회 UI 중 기존 수동 패널 숨김), `VirtualGameTeamProj.Build.cs`(UMG 의존성), `ProcessUIStateTests.cpp`, `ProcessUIStateScreenValidation.cpp`, 이 README입니다. 맵·위젯 에셋·프로젝트 설정은 변경하지 않았습니다.

아래는 UI 최초 제작 당시 기록이며, 위 연결 계약과 검증 기록이 현재 상태를 설명합니다.

---

# ProcessUIV2 — 첫 화면 구현 및 검증 상태

전체 공정도 위젯을 생성·컴파일·저장했습니다. 실제 장비 데이터·명령은 연결하지 않았습니다. 실제 입력과 화면 배치의 실행 검증은 미완료입니다.

## 생성한 에셋

- `/Game/ProcessUIV2/Widgets/WBP_ProcessUIV2` — 전체 화면
- `/Game/ProcessUIV2/Testing/BP_ProcessUIV2_Host` — PIE에서 전체 화면을 띄우는 독립 Actor
- `/Game/ProcessUIV2/Widgets/WBP_EquipmentPrototype`
- `/Game/ProcessUIV2/Testing/BP_ProcessUIV2_PrototypeHost`

마지막 두 에셋은 기존 단일 장비 시제품으로 보존했습니다. 현재 검증 대상은 WBP_ProcessUIV2입니다.

## 레벨 저장 없이 실행하기

UI는 특정 맵이나 Level Blueprint에 종속되지 않습니다. 전용 맵은 필수 조건이 아니며 생성하지 않았습니다. MCP에는 PIE 제어와 캡처는 있지만 임의 위젯 실행 및 마우스 입력 주입 도구는 없습니다.

1. 콘텐츠 브라우저에서 BP_ProcessUIV2_Host를 열어 클래스를 로드합니다.
2. 현재 레벨에서 PIE(Play)를 실행합니다. 기존 레벨의 플레이 자체가 기존 프로젝트 로직을 실행한다는 점을 고려하여 사용자가 실행 시점을 선택합니다.
3. 게임 콘솔에서 아래 명령을 한 번 실행합니다.

   `summon /Game/ProcessUIV2/Testing/BP_ProcessUIV2_Host.BP_ProcessUIV2_Host_C`

4. Host는 플레이 월드에만 생성되어 위젯을 추가하고 마우스 커서와 UI 입력 모드를 설정합니다. 에디터의 Stop 버튼으로 종료합니다. 반복 Summon은 위젯을 중복 생성하므로 새 PIE 세션에서 한 번만 실행합니다.

이 경로는 현재 플레이 컨트롤러가 게임 콘솔과 CheatManager Summon을 지원해야 합니다. 엔진 소스의 명령 구현은 확인했지만 이 프로젝트의 실행 중 지원 여부는 미검증입니다. 명령을 사용할 수 없으면 기존 레벨 설정을 바꾸지 말고 중단합니다. 대안은 저장하지 않을 새 빈 레벨에 Host를 배치해 Play한 뒤 저장 없이 닫는 수동 미리보기입니다.

기존 `/Game/ProcessUI/Maps/L_ProcessUI_Test`, `/Game/L_Main`, GameMode, Level Blueprint, 배치 Actor는 변경하지 않았습니다.

## 구현한 동작과 남은 실행 확인

- 공급 호퍼 A~F → 공통 칭량호퍼 → 수취호퍼 → 혼합기. 연결선에는 화살표 없음
- 장비에 2.5초 머무르면 이름·쉬운 설명·상태 표시
- 2.5초 전 이탈하면 취소; 다시 진입하면 처음부터 대기
- 클릭하면 정보창 숨김, 고정 패널 표시
- 커서를 고정 패널로 옮겨도 유지
- 다른 장비 클릭 시 같은 패널 내용 교체. 전용 닫기 버튼 또는 Esc로 닫힘. Esc 이외 키는 Unhandled 반환
- 패널 배경 Button이 입력을 소비하며 내부 ScrollBox와 버튼을 포함함
- 고정 패널 표시 중 호버 정보 억제
- 3D에서는 공정도 영역을 숨기고 미연결 관찰 안내만 표시. 같은 위젯과 월드를 유지하며 상태·선택값을 초기화하지 않음
- 2D 복귀 시 기존 패널·선택 유지. 카메라나 실제 모델은 아직 매핑하지 않음
- 장비 값은 데이터 없음·미확인·미연결로 표시
- 조작 버튼은 모두 비활성. 클릭으로 실제 상태를 바꾸는 구현 없음
- 작은 화면에서는 가로·세로 스크롤을 사용. 고정 최소 작업 영역 때문에 일부 내용은 스크롤 후 접근해야 함

위 항목은 그래프·속성 기준의 구현 설명입니다. 실제 호버 시간, 마우스 이탈·팝업 입력 차단, Esc, 화면 전환, 작은 화면 및 글자·연결선 겹침은 PIE에서 확인해야 합니다. 마지막 캡처는 자동 임포트 확인창에 가려졌습니다.

## 상태·명령 연결 지점

- `OnStateRequested(EquipmentId)`: 조회 요청 디스패처. 정지·조작 명령과 분리
- `ApplyEquipmentSnapshot(EquipmentId, StatusText, QuantityText, ValveText, SourceLabel, IsTestData)`: 현재 선택·호버 ID와 일치하는 표시만 갱신. 패널의 테스트 데이터는 테스트 모드로 표시하며, 공급자는 호버 상태 문구에도 테스트임을 명시해야 함
- `OnCommandIntent(EquipmentId, Command, Value)`: 향후 명령 의도 연결용 디스패처. 현재 버튼에서 발행하지 않으며 비활성 유지
- `OnManualInterventionRequested`: 수동 개입 정책 연결용. 자동 중단 → 전체 정지·밸브 닫기 요청 → 실제 완료 확인은 외부 제어 어댑터의 책임. 가짜 완료 처리 없음
- `OnObservationViewRequested(ShowObservation)`: 같은 월드의 실제 관찰 카메라 연결용. 현재 카메라 변경 로직 없음

WH101/RH-201/MX-301은 표시 참고값이며 Actor 매핑은 비어 있습니다. 상태 공급자, 명령 처리자, 실제 정지 확인, 관찰 카메라·모델 연결은 남은 작업입니다.

## 데이터와 검증 기록

- 저장소 `ProcessUIV2/Data/equipment.json`: 장비 ID·표시 이름·설명·연결 관계. Blueprint에도 장비 메타데이터 배열을 저장했습니다.
- 이번 버전의 장비 수와 이벤트는 정의 데이터로 제작한 에셋에 반영되어 있습니다. JSON 수정만으로 런타임 장비가 자동 증감하지는 않습니다. 공급 호퍼 증설 시 위젯과 이벤트 재생성이 필요합니다.
- `ProcessUIV2/Tools/audit_mcp.py`: MCP ProgrammaticToolset 환경에서 실행하는 구조 검증 스크립트
- `ProcessUIV2/Diagnostics/widget_graph_snapshot.json`: 그래프·위젯·속성 검증 스냅샷
- 검증 결과: 장비 9개, 연결된 버튼 이벤트 31개, 단일 호버 타이머 2.5초, 비활성 명령 버튼 8개, 초기 팝업 숨김, 미연결 값 확인. 위젯·Host 컴파일 및 명시적 에셋 저장 성공

## 도구 제약

한국어 에디터에서 Blueprint DSL 도구가 실행 핀을 자동 연결하지 못했습니다. 실제 핀 목록을 조회하고 MCP connect_pins로 실행 연결을 구성했습니다. 컴파일 성공과 런타임 동작 성공은 구분합니다.

함수의 DisplayName 입력으로 발생한 빈 함수명 문제는 입력명을 EquipmentLabelInput/EquipmentDescriptionInput으로 바꿔 재구성했습니다. 실제 재생성된 그래프 이름은 BeginHover_0, SelectEquipment_0입니다. 플러그인 소스나 설정은 수정하지 않았습니다.
