# 작업 09 — Unreal 단일 입자 연결과 계산 조작

완료일: 2026-10-21. PowderCore 0.9.0. 브랜치: PowderSimulation. 커밋·푸시는 하지 않았다.

## 구현

- `Physics/PowderCore/include/powder/session.hpp`: 고정 dt 계산 세션. 시작/일시정지, 정확히 한 단계 진행, 초기 상태 복원, 접촉 이력 삭제, 오류 발생 시 정지. 실패한 단계는 물리 시간에 포함하지 않는다.
- `Source/VirtualGameTeamProj/DEMCore_*.cpp`: 독립 코어 소스를 Unreal 모듈에서 그대로 컴파일한다. 계산식을 별도로 복제하지 않는다. 예외 처리를 활성화하고 translation unit별로 빌드한다.
- `APowderDEMTest`: 단일 구를 자체 DEM 좌표·자세로 표시한다. Chaos 시뮬레이션·중력·충돌을 모두 끈다. 표시만 cm 단위를 사용한다.
- 실행 및 R 초기화 시 현재 레벨의 PowderRig 큐브 월드 변환을 읽어 SI 정적 상자를 구성한다. 작업 08의 저장 스냅샷을 로드하는 방식과 달리 PIE에 반영된 실제 도형 변환을 직접 읽는다. 미지원 메시, 움직이는 Chaos 벽, 잘못된 물성/시간 간격은 오류로 정지한다.
- 기존 GameMode는 300개 Chaos 입자 생성 대신 단일 DEM 액터와 새 HUD를 사용한다. 기존 Chaos 클래스 코드는 보존하지만 기본 실행 경로에서는 사용하지 않는다.
- HUD에 물리 계산 시간·단계 수, 초기화 후 실제 경과 시간, 계산에 소비한 CPU 실행 구간의 누적 벽시계 시간을 따로 표시한다. 여기서 Compute는 프로파일러의 CPU 사이클 합계가 아니라 계산 호출 전후의 경과 시간이다.
- 카메라 조작은 유지하고 F 초점을 DEM 입자 위치에 연결한다. 계산 일시정지는 전체 월드 일시정지가 아니므로 카메라를 계속 조작할 수 있다.

## 조작

L_Main에서 Play를 누르면 초기화된 일시정지 상태로 시작한다. 뷰포트를 클릭한 뒤:

| 키 | 기능 |
|---|---|
| Space | 시작 / 일시정지 / 재개 |
| N | 계산을 일시정지하고 고정 dt 한 단계만 진행 |
| R | 입자·시간·속도·회전·접촉 이력 초기화, 현재 시험장 변환 다시 읽기 |
| F | 현재 입자에 카메라 초점 |
| Home / T | 전체 시점 / 관 근접 시점 |
| 우클릭 + WASD/QE | 기존 비행 카메라 이동 |

기본값은 작업 08의 **수치 시험용 가상 입자**다: 지름 0.2m, 밀도 약 238.732kg/m³(질량 1kg), 초기 위치 (0,0,550)cm, 중력 9.81m/s², 선형 kn=4e6N/m, cn=800Ns/m, kt=1e6N/m, ct=200Ns/m, 마찰계수 0.3. 실제 분말로 보정된 값은 아니다.

DEM 액터의 Details → DEM에서 지름·밀도·초기 위치·강성·감쇠·마찰·dt를 편집하고 R로 반영할 수 있다. PIE에서 편집한 값은 세션 종료 후 영구 보존되지 않는다. StepsPerFrame은 실행 중 다음 프레임부터 반영한다.

## 시간 처리와 범위

물리 dt 기본값은 2e-6초, 프레임당 기본 계산 예산은 500step(설정 범위 1~5000)이다. DeltaSeconds를 물리 dt로 쓰지 않고, 현실 시간에 뒤처졌다는 이유로 물리 단계를 생략하지 않는다. 물리 시간은 완료 단계 수 × 고정 dt다.

프레임률이나 계산 예산이 달라지면 **현실 시간당 계산 진도**는 달라질 수 있다. 동일한 물리 단계에 도달했을 때 계산 상태는 같다. 이번 단계는 게임 스레드에서 동기 계산하므로 복잡한 접촉에서 화면 응답이 느려질 수 있다. 백그라운드 실행은 다음 작업 10이다.

벽 형상은 초기화 시점의 정적 스냅샷이다. 실행 중 벽을 움직여도 자동 추적하지 않으며 R로 다시 읽어야 한다. 런타임 GUI는 선형 모델을 사용한다. 코어의 Hertz 모델은 유지하지만 이번 화면에 모델 선택 UI는 추가하지 않았다.

## 검증

1. 독립 Release 빌드 및 CTest 8개 모두 통과: 기존 7개 + session_controls. 총 약 34초.
2. Python CLI·운동·평면 회귀 검사 3개 모두 통과.
3. UnrealEditor Development 빌드 성공.
4. Unreal 자동 검사 `Powder.DEM.SingleParticleBridge` 성공:
   - Chaos 시뮬레이션·중력·충돌 비활성 확인.
   - 초기 일시정지, N 정확히 1step, 시작/정지, R 시간·접촉 이력 삭제 확인.
   - 접촉 상태에서 30회 Tick×400step과 120회 Tick×100step 비교. 각 Tick의 DeltaSeconds도 1/30, 1/120으로 다르게 적용.
   - 양쪽 12000step, 물리 시간 동일. 계산 좌표 차이 0, 표시 좌표 차이 0, 표시와 계산 좌표 차이 0.
   - 최종 z=9.9997476267069505cm. 이는 접촉 중인 합성 구–평면 시험이다.
   - 벽 이동 후 R 초기화 시 새 형상으로 다시 계산함을 확인.
5. 실제 에디터 L_Main에서 Play: 새 HUD, 구 1개, N 이후 Steps=1 및 Simulation=0.000002s, Space 실행/정지, F 초점, R 초기화 모두 직접 확인했다. 약 2.1초의 물리 계산까지 오류 없이 진행했다. 마지막에는 R로 초기화하고 F로 입자를 보여 주는 일시정지 상태로 남겼다.

30/120 검사는 실제 렌더러의 프레임률 측정이 아니라 같은 물리 단계에 대한 Tick 분할 및 DeltaSeconds 독립성 검사다. 다른 플랫폼·컴파일러 사이의 비트 단위 동일성까지 보장하지 않는다.

## 발견한 문제와 수정

- UHT 생성 생성자가 불완전한 구현 타입의 기본 deleter를 인스턴스화하여 컴파일 실패: 명시적 deleter를 정의하여 소유권과 소멸을 유지했다.
- 열린 에디터의 DLL 잠금으로 링크 실패: 에디터 UI로 정상 종료 후 빌드하고 다시 열었다. 강제 종료·미저장 내용 폐기는 하지 않았다.
- 초기 엔진 검사에서 프레임 분할에 따른 표시 좌표 차이 약 2.05e-6cm가 발생했다. 계산 좌표 차이는 0이었다. 일반 컴포넌트 이동의 미세 변화 처리를 피하도록 `SetWorldLocationAndRotationNoPhysics` 및 명시적 변환 전파를 사용했다. 허용오차 1e-10cm를 유지한 채 재검증하여 차이 0을 확인했다.
- 작업 08 CMake의 geometry.cpp 중복 항목도 정리했다.

## 재실행

프로젝트 루트에서 기존 CMake/CTest 및 Python 검사 명령을 사용한다. 엔진 검사는 에디터를 닫고 빌드 후:

```powershell
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' "$PWD/VirtualGameTeamProj.uproject" -unattended -nullrhi -nosound '-ExecCmds=Automation RunTests Powder.DEM.SingleParticleBridge' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$PWD/Saved/DEM_task09_automation"
```

결과는 `Saved/DEM_task09_automation/index.json`, 로그는 `Saved/Logs/DEM_task09_automation.log`에 기록했다. 자동 검사는 종료 코드만 보지 말고 테스트 Result=Success도 확인해야 한다.

다음은 **작업 10 — 백그라운드 계산과 다입자 표시**다.
