# 독립 DEM 입자 설정 사용 안내

UE 5.8.2 / `DEMPrototype` 플러그인의 `UDEMParticleSettings` 기반 Data Asset입니다. 특정 맵이나 액터 없이 생성·복제·공유합니다. 현재 `BP_DEMSimulation`은 이 에셋을 읽지 않습니다. 기존 BP 설정을 바꿔 주는 기능은 아닙니다.

## 생성·복제·편집·저장

1. 프로젝트의 C++ Editor 타깃을 빌드한 뒤 에디터를 엽니다. 이미 열려 있었다면 새 C++ 클래스를 사용하기 위해 저장 후 재시작합니다.
2. 콘텐츠 브라우저에서 `Content/DEM/Settings`로 이동합니다. 우클릭 → Miscellaneous(기타) → Data Asset → **DEM Particle Settings** (`DEMParticleSettings`)를 선택합니다.
3. 또는 `DA_DEMParticle_Default`를 우클릭 → Duplicate(복제)하여 별도 이름으로 저장합니다. 팀별 변형은 기본 에셋 대신 복제본에서 만듭니다.
4. 더블클릭하여 Physics / Contact / Display 항목을 편집하고 Save 합니다. 우클릭의 Validate Assets(에셋 검증)으로 검증합니다. 값이 잘못되면 오류에 해당 필드가 나옵니다. UI 입력 제한을 우회한 값도 스냅샷 생성 시 다시 검사합니다.

물리 수치는 모두 double입니다. 반지름은 m, 밀도는 kg/m³, kn/kt는 N/m, 감쇠비와 마찰계수는 무차원입니다. 기본 반지름 0.01, 밀도 1000, kn 500000, kt 250000, 감쇠비 각각 0.2, 마찰계수 각각 0.4는 **기존 DEM 시험용 가정값이며 실제 분말의 측정 물성이 아닙니다.** 반지름·강성을 변경해도 기존 시뮬레이션 dt의 안정성은 자동 보장되지 않습니다.

지름·체적·질량·관성은 입력하거나 저장하지 않습니다. `Get Derived Properties`로 계산합니다. 기본 질량은 약 0.004188790204786391 kg, 관성은 약 1.6755160819145568e-7 kg·m²입니다.

## 메시와 물리 형상

`Sphere Mesh`는 기존 `/Game/DEM/SM_DEMSphere`를 선택합니다. `Material Override`는 선택 사항이며 비워 두면 메시의 재질을 사용합니다. 표시 경계에서만 m→cm로 변환합니다. 균일 스케일은 `물리 반지름(m) × 100 / 원본 메시 반지름(cm)`입니다. 50 cm 고정 가정을 하지 않습니다.

원본 반지름은 메시 render-data bounds의 가장 큰 반외곽 크기입니다. 사용자 지정 bounds 확장은 포함하지 않습니다. 중심 오차는 반지름의 0.01% 이하, 축별 외곽 크기 차이는 1% 이하를 허용합니다. 원점 중심의 구 메시를 사용하세요. 이러한 경계 검사만으로 메시가 완전한 구임을 증명하지 못합니다(예: 정육면체도 경계 검사는 통과할 수 있습니다). 다각형 근사·LOD·재질의 World Position Offset 등에 의한 실제 표면 오차는 별도 검토해야 합니다. 구를 변형하는 재질은 사용하지 마세요.

메시·재질 교체는 해석 형상을 바꾸지 않습니다. DEM 해석 형상은 항상 구이며 별도 렌더 크기 배율은 없습니다. 미래 생성기는 계산된 균일 스케일을 사용하고 부모 Transform의 추가 스케일로 크기를 바꾸지 않아야 합니다.

## 미래 생성기의 연결 지점

생성기는 `UDEMParticleSettings` 에셋 참조 하나를 가지며 여러 생성기가 같은 에셋을 선택할 수 있습니다. 생성 또는 Reset 때 검증한 스냅샷을 한 번 복사해 실행에 사용합니다. 실행 중 공유 에셋을 직접 수정하지 않고, 편집 변경은 **다음 생성/Reset**에서 다시 읽습니다. 현재 범위에서는 생성기를 구현하지 않았습니다.

C++ (사용 모듈 Build.cs에 `DEMPrototype` 의존성 추가):

```cpp
#include "DEMParticleSettings.h"

// 미래 소비자 클래스의 멤버 예시:
UPROPERTY(EditAnywhere, Category="DEM")
TObjectPtr<UDEMParticleSettings> ParticleSettings;

UPROPERTY(Transient)
FDEMParticleRenderSnapshot ActiveSettings; // 공유 메시/재질의 GC 참조 유지

// 생성/Reset의 게임 스레드에서:
TArray<FText> Errors;
FDEMParticleRenderSnapshot Candidate;
if (!ParticleSettings || !ParticleSettings->TryCreateRenderSnapshot(Candidate, Errors))
{
    // Errors를 사용자에게 표시하고 생성을 중단. 실패 출력은 0/null이며 사용 금지.
    return;
}
ActiveSettings = Candidate;
// 렌더: SphereMesh, MaterialOverride, FVector(UniformScale)
// 물리: ActiveSettings.Physics의 SI 수치와 Derived 사용
```

렌더 없는 해석은 `TryCreatePhysicsSnapshot`을 호출합니다. 이 경로는 메시가 없어도 유효하며 메시·액터·레벨 참조가 없는 값 구조체를 반환합니다. 워커 스레드에는 이 복사본을 전달하세요. 물리 스냅샷도 공유 에셋을 계속 읽지 않습니다.

Blueprint: `DEM Particle Settings Object Reference` 변수에 에셋 선택 → 생성/Reset 이벤트에서 `Try Create Render Snapshot` → 반환 bool로 Branch → 성공일 때 스냅샷 변수 저장 → Break 구조체로 사용합니다. 헤드리스라면 `Try Create Physics Snapshot`, 조회만 한다면 `Get Derived Properties` 또는 `Validate Physics` / `Validate Display`를 사용합니다. 실패 시 Errors 배열을 표시하고 출력을 소비하지 않습니다. 스냅샷/설정 필드는 Blueprint 읽기 전용입니다.

`IsDataValid`는 물리 오류와 지정된 잘못된 메시를 오류로 보고합니다. 메시 미지정은 물리 전용 에셋을 허용하기 위해 경고입니다. 표시 스냅샷은 메시 미지정도 실패합니다. 파생값과 역질량·역관성은 유한한 양의 정규 double 범위만 허용하며, subnormal/0/Inf가 되면 오류로 반환합니다. 표시 스케일은 최종 float 렌더 경계에서도 검사합니다. 이는 범용 수치 검증이며 특정 솔버의 dt·접촉 조합·실행 안정성 검증을 대신하지 않습니다.

입자 수, seed, 생성 영역·위치·초기속도는 미래 생성기의 책임입니다. 중력, dt, 실행 속도는 시뮬레이션의 책임입니다. 입자별 위치·속도·힘·접촉 이력은 이 에셋에 저장하지 않습니다. 서로 다른 에셋을 여러 개 생성할 수 있지만, 서로 다른 물성 간 강성·마찰 합성 규칙이나 혼합 해석 지원을 의미하지는 않습니다.

## 팀 공유

`Content/DEM/Settings`의 에셋·안내, 참조하는 `Content/DEM/SM_DEMSphere.uasset`, `Plugins/DEMPrototype`의 `.uplugin` 및 Source를 함께 공유합니다. 별도 메시·재질을 선택하면 그 종속 에셋도 포함합니다. 런타임 설정은 기존 DEMPrototype 모듈(Core/CoreUObject/Engine)에 있고 생성·검증 commandlet은 DEMPrototypeEditor(UnrealEd 등)에 있습니다. MCP는 실행 의존성이 아닙니다.

`Binaries`, `Intermediate`, `DerivedDataCache`, `Saved`는 공유하지 않고 팀원 환경에서 재빌드합니다. `.uasset`은 바이너리이므로 같은 에셋의 동시 편집을 피하고 팀별 복제본 또는 저장소 잠금 정책을 사용합니다. 실행 중 재질/메시 리소스 자체를 변경해도 스냅샷이 깊은 복사를 제공하는 것은 아닙니다. 공유 리소스도 불변으로 사용하세요.

자동 검증 재현 명령(Windows PowerShell, 프로젝트 루트):

```powershell
.\Plugins\DEMPrototype\Tools\ParticleSettings.ps1 -Task Build
.\Plugins\DEMPrototype\Tools\ParticleSettings.ps1 -Task Create
.\Plugins\DEMPrototype\Tools\ParticleSettings.ps1 -Task Verify
```

Create는 파일이 있으면 현재 상태만 읽고 덮어쓰지 않습니다. Verify는 별도 Unreal 프로세스에서 로드하며 모든 잘못된 입력 시험은 transient 복제본/메시에 수행합니다. 기록은 `Plugins/DEMPrototype/ParticleSettingsValidation`에 남습니다. 검증 결과 및 수동 검증 여부는 해당 폴더의 `REPORT_KO.md`를 참고하세요.
