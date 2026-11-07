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
