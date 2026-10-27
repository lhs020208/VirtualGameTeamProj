# BP_DEMEmitter 사용 안내

이 생성기는 **입자 데이터를 준비하고 구 메시를 정지 표시**합니다. 현재 중력 낙하, 접촉 운동, 회전 적분을 실행하지 않습니다. 초기속도는 다음 DEM 해석기 연결을 위한 데이터입니다.

## 레벨에서 사용

1. 원래 `VirtualGameTeamProj`의 Editor 모듈을 빌드하고 새 에디터 프로세스로 프로젝트를 엽니다.
2. 콘텐츠 브라우저의 `Content/DEM/Emitters/BP_DEMEmitter`를 원하는 레벨로 드래그합니다. 특정 맵·Level Blueprint·GameMode·HUD·카메라·DEMSimulationActor 설정은 필요 없습니다.
3. Details의 `Particle Settings`에 `/Game/DEM/Settings/DA_DEMParticle_Default` 또는 복제한 설정 에셋을 선택합니다. 기본 BP에는 이 기본 에셋이 지정되어 있습니다.
4. `Particle Count`, `Columns X/Y`, `Surface Gap`(m), `Initial Linear Velocity`(m/s), `Initial Angular Velocity`(rad/s)를 지정합니다. 이동·회전 도구로 위치와 방향을 지정합니다. 청록색 편집기 화살표는 생성기 원점에서 로컬 +Z 적층 방향을 가리킵니다.
5. `Generate on BeginPlay`가 켜져 있으면 Play 시 정확한 요청 개수를 한 번 생성합니다. 입자는 그 위치에 정지합니다. 생성 후 생성기를 움직여도 이미 생성된 입자의 월드 위치는 바뀌지 않습니다. 일반 레벨의 카메라/조명/시야는 팀 레벨에서 제공해야 화면으로 볼 수 있으며, 생성기 자체의 데이터 생성에는 필요 없습니다.

입자 크기는 **입자 설정 에셋의 Radius**로 바꿉니다. Actor Scale은 부모를 포함한 월드 Scale `(1,1,1)`만 지원합니다. 비단위 Scale이면 생성하지 않고 오류를 표시합니다. 밀도·강성·감쇠·마찰·메시·재질도 설정 에셋에서만 편집합니다. 여러 생성기가 같은 설정을 재사용할 수 있습니다.

## Generate / Clear / Preview

- **Generate**: 현재 설정을 다시 읽고 이전 데이터를 완전히 대체합니다. 누적 방출이 아닙니다. Blueprint에서 성공 여부가 필요하면 **Try Generate**의 bool을 확인합니다.
- **Clear**: 이 생성기의 데이터와 표시만 제거합니다.
- **Editor Preview**: 편집 시 초기 배치를 표시합니다. 최대 2,048개만 표시하고 `Generation Status`에 `PREVIEW 표시수 / requested 요청수`를 보여줍니다. 실제 Particle Count는 바꾸지 않으며 물리용 데이터를 내보내지 않습니다.
- **Refresh Preview**: 공유 설정 에셋을 편집·저장한 다음 선택한 생성기의 이 버튼을 눌러 갱신합니다. 생성기 속성/변환 편집도 construction 미리보기를 갱신합니다. 실행 중에는 이 함수가 기존 burst를 덮어쓰지 않습니다.
- Play 시작 시 미리보기를 제거하고 실제 요청 개수로 새로 생성합니다. Generate on BeginPlay를 끄면 Play 시작 시 미리보기만 제거합니다.
- `Generation Status`, `Generated Count`, `Visible Instance Count`, `Preview Only`로 상태를 확인합니다. 오류는 Output Log의 `LogDEMEmitter`에도 한 번의 생성 시도마다 기록됩니다. 프레임별 로그는 없습니다.

편집기에서 Generate를 직접 누르면 전체 요청 개수를 생성할 수 있습니다. 이후 액터 속성/변환 편집은 다시 미리보기 모드로 갱신할 수 있습니다. 기본 에셋을 직접 수정하기보다 팀별 복제본으로 크기/물성을 편집하세요. 실행 중 공유 설정 변경은 기존 생성 결과에 소급되지 않으며 **다음 Generate/Play 생성**에서 적용합니다.

## 배치와 좌표 계약

격자 pitch는 `2 × radius + gap`입니다. 인덱스 i에 대해 X가 가장 빨리 증가하고, Y, Z층 순입니다. X/Y는 전체 `Columns X × Columns Y` 직사각형을 중심 정렬합니다. 마지막 행/층이 덜 차도 원점 기준을 다시 옮기지 않습니다. 첫 층의 입자 **중심**은 로컬 `Z=radius`이며 입자 바닥이 로컬 Z=0 평면에 닿습니다. 이후 층은 pitch씩 증가합니다. 격자 내부의 초기 겹침을 피하며 다른 생성기/설비와의 겹침 검사는 하지 않습니다.

속도 입력은 생성기 로컬 Unreal 축 기준입니다. 선속도와 각속도를 생성 시 액터 회전으로 월드 축에 돌린 뒤 기존 DEM 규약에 맞춥니다:

- Unreal 월드 위치 cm → DEM 위치 m: `(x, -y, z) / 100`
- Unreal 월드 선속도 m/s → DEM: `(vx, -vy, vz)`
- Unreal 월드 **축 벡터** 각속도 rad/s → DEM: `(-wx, wy, -wz)`
- Unreal 자세 quaternion → DEM: `(-qx, qy, -qz, qw)`

DEM 데이터는 오른손 좌표계, +Z 위, double SI입니다. Unreal의 FVector/FQuat도 이 UE 버전에서 double인 `FVector3d/FQuat4d`를 명시해 저장합니다. 표시 경계에서만 cm를 쓰고, 기존 설정 API가 계산한 원본 구 반지름 기반 균일 스케일을 적용합니다. ISM은 생성 당시의 절대 기준 변환을 유지합니다. 추가적인 ISM/부모 스케일로 입자 크기를 변경하지 마세요.

## 범위 제한과 오류

실제 burst는 생성기당 **0..100,000개**, 각 Columns는 **1..100,000**입니다. 0개는 유효한 빈 burst입니다. 미리보기 상한과 달리 실제 상한 초과 요청은 오류이며 자동으로 잘라내지 않습니다. 격자 축별 로컬 범위는 10,000m, 생성기 원점은 축별 ±1,000,000m 이내로 제한합니다. 입자 크기가 격자/월드 좌표의 부동소수점 정밀도에 비해 지나치게 작아도 오류입니다. 이러한 제한은 큰 할당과 표시 정밀도 손실을 방어하기 위한 것이며 실제 공정 크기의 권장값이 아닙니다.

잘못된 설정·메시·NaN/Inf·음수 간격·비단위 Scale·범위 초과는 생성 실패입니다. 실패하면 기존 표시와 데이터도 제거하여 이전 결과를 새 성공 결과로 오인하지 않게 합니다. 연속 방출·수명·풀링·입자별 Actor/UObject/Tick/Chaos Body는 없습니다. 생성기를 삭제하면 이 생성기가 소유한 데이터와 ISM 표시를 정리합니다. 다른 생성기는 수정하지 않습니다.

## 미래 공통 DEM 해석기 연결

런타임 모듈에서 `DEMPrototype` 의존성과 `#include "DEMEmitter.h"`를 추가합니다. 생성 직후 게임 스레드에서:

```cpp
FDEMEmitterBurst Burst;
if (Emitter && Emitter->CopyGeneratedBurst(Burst))
{
    // Burst.Settings: 검증된 물성 값 스냅샷 (메시/액터/에셋 참조 없음)
    // Burst.Particles: 연속 배열, LocalIndex / PositionMeters /
    // LinearVelocityMetersPerSecond / AngularVelocityRadiansPerSecond / Orientation
    // 공통 해석기가 전역 ID를 부여하고 자신의 배열로 복사/이동하여 보관합니다.
}
```

미리보기·Clear·생성 실패에서는 false와 빈 출력을 반환합니다. LocalIndex는 해당 생성기와 현재 burst 내부의 인덱스이며 전역 ID가 아닙니다. Generate는 새 burst로 교체하므로 외부 해석기는 재생성/파괴 시 이전 전역 ID 처리 정책을 별도로 정의해야 합니다. 복사된 배열은 생성기 파괴 후에도 소비자가 소유합니다. 이번 작업은 소유권 이전 후의 해석기 수명·혼합 물성 규칙을 구현하지 않습니다. 공유 메시/재질도 깊게 복제하지 않습니다.

## 공유와 재검증

- 공유: `Content/DEM/Emitters`의 BP/안내, 선택한 `Content/DEM/Settings` 에셋, 참조 메시/재질, `Plugins/DEMPrototype`의 `.uplugin`과 Source. 기존 설정 에셋만 전달하거나 BP만 전달하면 네이티브 클래스가 없어 로드할 수 없습니다.
- `Binaries`, `Intermediate`, `Saved`는 각 환경의 빌드 산출물이며 공유하지 않습니다.
- 원래 프로젝트 루트의 PowerShell에서 에디터를 저장·종료한 후 실행합니다:

```powershell
.\Plugins\DEMPrototype\Tools\Emitter.ps1 -Task Build
.\Plugins\DEMPrototype\Tools\Emitter.ps1 -Task Create
.\Plugins\DEMPrototype\Tools\Emitter.ps1 -Task Verify
```

Create는 이미 있는 BP를 덮어쓰지 않고 부모 클래스/기본 설정을 검사합니다. Verify는 원래 프로젝트를 새 프로세스로 열고 저장하지 않는 임시 월드에서 검사합니다. `L_Main`, `DEM_Demo`와 기본 시작 맵을 변경하지 않습니다. 실제 결과와 수동 미검증 사항은 `Plugins/DEMPrototype/EmitterValidation/REPORT_KO.md`를 확인하세요.
