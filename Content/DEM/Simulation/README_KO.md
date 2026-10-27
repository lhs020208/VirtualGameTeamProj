# 공통 DEM 사용·협업 계약

원래 프로젝트 빌드·에셋 생성·새 프로세스 자동 검증을 완료했다. 실제 Play 화면에서 검게 보이던 문제를 시험 맵의 관찰 카메라 자동 활성화와 고정 노출(EV100=0)로 수정했다. `DEM/DEM_Integration`을 열면 관리자·200입자 생성기·시험 바닥/벽이 준비되어 있다. 자세한 검증 결과는 `Plugins/DEMPrototype/CommonValidation/REPORT_KO.md` 참조.

현재 `DA_DEMParticle_Default`의 반지름은 **0.005m**이며 사용자 값을 보존했다. 자기 레벨에서 이 에셋을 선택하면 DEMWorld의 **Fixed dt를 0.000005s 이하**로 설정한다. 기본 dt=0.00001s는 이 작은 입자의 보수적 제한을 초과해 시작이 거부된다. Integration은 별도 `DA_DEMParticle_IntegrationTest`(반지름 0.01m)를 사용하므로 dt=0.00001s 시험과 대응한다.

## 자기 레벨에서 사용

1. `DEM/Simulation/BP_DEMWorld` 하나를 배치한다. 관리자 Transform은 해석 공간에 영향을 주지 않는다.
2. `DEM/Emitters/BP_DEMEmitter`를 배치하고 Particle Settings, Particle Count, Columns X/Y, Surface Gap, 초기속도를 지정한다. 생성기가 여러 개면 Generation Order를 0, 1, 2처럼 서로 다르게 지정한다.
3. `DEM/Boundaries/BP_DEMPlaneBoundary`로 바닥·벽을 배치한다. Boundary 컴포넌트의 Boundary ID를 면마다 다르게 지정하고 Width/Height(m)로 크기를 정한다. 로컬 XY가 면이고 +Z 화살표가 기준 법선이다. 액터·부모 Scale은 모두 단위 Scale이어야 한다.
4. Target Simulation을 관리자에 지정하거나 비워 둔다. 비워 둔 경우 월드에 관리자가 정확히 하나일 때만 자동 연결한다. 월드당 관리자 하나만 지원한다.
5. Generate on BeginPlay와 관리자의 Auto Start를 켜고 Play한다. 별도 GameMode/HUD/Level Blueprint/DEMSimulationActor는 필요 없다. 일반 Static Mesh의 Chaos 충돌은 DEM 경계로 등록되지 않는다.

기본 크기는 설정 에셋의 Radius(m)로 변경한다. 물성은 공유 에셋에서 읽고 생성 시 스냅샷으로 복사한다. 실행 중 에셋을 바꿔도 기존 입자에는 적용되지 않는다. 정지 미리보기는 생성기의 Refresh Preview로 갱신한다. 연결된 생성기의 Generate/Clear는 독립 변경을 거부하고 관리자를 정지시켜 공통 Reset을 요구한다.

관리자가 없으면 기존 생성기는 정지한 입자만 표시한다. 관리자가 연결되면 운동 데이터는 관리자가 소유하고 기존 생성기의 ISM은 표시만 맡는다. 생성기를 이동해도 등록된 입자는 따라가지 않는다. 생성기·경계 삭제/추가, 순서·대상 변경, 경계 이동은 Reset 필요 상태로 정지한다.

실제 콘텐츠 브라우저 드래그 배치, Play, 200입자 낙하·퇴적 화면 확인을 완료했다. 최종 GUI 시험은 약 100초 동안 해석시간 5.019초까지 지연 정지 없이 진행했다. 드래그 시험으로 추가한 액터는 Undo로 제거했다.

## Integration 화면 확인

`DEM_Integration`을 열고 Play를 누르면 시험 카메라로 자동 전환된다. 이 카메라는 시험 맵의 보기 편의 기능이며 공통 해석기의 필수 의존성이 아니다. 일반 레벨에는 원하는 카메라를 사용한다.

이 맵은 실제 GUI에서 관찰하기 쉽도록 Speed Multiplier=**0.05**, Max Substeps=**10000**, Fixed dt=**0.00001s**로 저장했다. 실시간보다 20배 느린 재생이므로 해석시간 5초는 약 100초가 걸린다. 컴퓨터 성능이나 에디터 백그라운드 제한에 따라 더 느려질 수 있다. 지연 상한 정지는 고장으로 숨기지 않으며 Status에서 확인한다. 이전 0.1배속/2000 substep GUI 실행의 지연 정지 기록도 보존했다.

## 실행 제어와 Reset

Start/Pause/Step/Reset은 관리자 Details 또는 Blueprint에서 호출한다. Step은 정확히 한 Fixed dt만 수행하며 누적 지연이 있으면 그중 한 dt를 소비한다. Reset은 현재 생성기 Transform·현재 설정을 다시 읽어 초기 배열을 재생성하고 시간·접촉 이력·외력을 지우며 일시정지한다. 설정을 바꾸지 않았다면 동일 초기 위치/속도/순서가 재현된다. 생성기는 규칙 격자라 seed가 없다. 기존 레거시 솔버의 seed 경로는 유지한다.

렌더 DeltaTime은 accumulator에만 더한다. 속도배율은 dt와 별개다. Max Substeps 한도에서 계산하지 못한 시간은 남겨 둔다. Maximum Lag에 도달하면 시간을 버리거나 dt를 늘리지 않고 정지한다. Step으로 지연을 소모하거나 Maximum Lag를 늘린 뒤 속도배율/Max Substeps를 조정해 Start한다. Reset만 명시적으로 지연을 지운다. 기본 Max Substeps=500은 저속 진행/지연 정지가 일어날 수 있는 보수적 예산이다.

표시 주기는 Display Hz이며 물리 결과와 독립이다. 프레임당 최대 한 번 ISM 변환을 일괄 갱신하고 물리 step마다 인스턴스를 재생성하지 않는다. 카메라 밖에서도 계산한다. 상태·시간·입자/경계/접촉/후보 수·속도·에너지·겹침비·step 시간·지연은 관리자 Details/Blueprint 조회로 확인한다.

## 물성·ID·좌표·소유권

공통 코어는 `dem::Solver` 하나이며 외부 초기 배열은 `resetExternal`로 전달한다. 기존 접촉 함수를 재사용한다. 선형 스프링–댐퍼 법선력과 접선 변위 이력 마찰이며 Hertz–Mindlin이 아니다. 적분은 고정 dt semi-implicit Euler와 quaternion 정규화다.

한 실행에서는 반지름, 밀도, kn, kt, 법선/접선 감쇠비, 입자/벽 마찰계수 8개 값이 모든 생성기에서 정확히 같아야 한다. 다른 에셋이어도 이 값이 같으면 허용한다. 메시/재질은 달라도 된다. 혼합 접촉 물성 규칙은 없다. 전체 최대 10,000입자, 경계 컴포넌트 최대 256개이며 초과는 오류다. 접촉/후보 저장 공간을 초기 할당하고 초과하면 정지한다.

전역 ID는 Generation Order 정렬 후 로컬 인덱스 순으로 0부터 부여한다. `FirstGlobalID + LocalIndex`가 매핑이다. 이 ID는 현재 Reset 세대에만 유효하다. Reset에서 참가자/개수가 바뀌면 매핑도 바뀐다. `ADEMWorld::GetSolver()`의 const 결과에서 배열을 복사할 수 있다. 원본 배열 참조를 다음 Reset 이후 보유하지 않는다. 연결 중 `CopyGeneratedBurst`는 오래된 초기 상태를 해석 상태인 것처럼 반환하지 않는다.

코어는 double SI(m, kg, s, N, rad/s), 우수 Z-up 좌표를 사용한다. `DEMCoordinates.h`가 변환의 단일 경계다. UE 월드 위치 cm는 0.01배 후 (x,-y,z), 선속도·힘·법선은 (x,-y,z), 각속도·토크는 (-x,y,-z), quaternion은 (-x,y,-z,w)다. 입력 초기속도는 생성기 로컬 축이며 먼저 생성기 회전을 적용한다. 관리자 외력 API 입력은 UE 월드 축이지만 단위는 N/N·m다.

## 유한면 및 모델 담당자 계약

면은 두께 없는 양면 직사각형이다. 입자 중심에서 직사각형 최근접점까지의 거리로 접촉을 판정한다. 내부는 면 법선, 모서리/꼭짓점은 최근접점에서 중심으로 향하는 법선을 사용한다. 거리가 0이면 기준 법선을 사용한다. 큰 dt/고속에서는 면을 관통할 수 있으며 연속 충돌 검사는 없다.

동일 컴포넌트 중복 등록은 제거한다. 같은 위치·축·크기·지지력 표식의 중복 면은 한 번 계산한다. 다른 Boundary ID라도 기하가 정확히 같은 경우다. 반대 축 표현 등 일반적인 기하 동치까지 판별하지는 않는다. 접합부의 서로 다른 면에는 각각 접촉력이 발생할 수 있다. 중복/겹치는 접촉면을 만들지 않는다. 기존 용기의 끝단 평면 단순화와 새 최근접점 유한면은 다른 형상이므로 끝단 결과를 동일 회귀 기준으로 비교하지 않는다.

모델 담당자는 접촉할 고정 부품을 분리하고 실제 치수·단위·피벗·월드 배치·면 법선을 제공한다. 직사각형 평면으로 표현 가능한 부분에 Boundary 컴포넌트를 추가한다. 외관 메시를 넣는 것만으로 곡면·삼각형 메시 DEM 접촉이 생기지 않는다. 움직이는 벽, 회전 날개, 임의 메시/곡면, 비구형, 응집/파쇄/구름저항, GPU/병렬화는 미지원이다.

## 외력 확장 및 공유

`SetExternalLoad(GlobalID, ForceN, TorqueNm)`은 다음 고정 step부터 중력·접촉력과 별도로 합산한다. 값은 교체/`ClearExternalLoads`/Reset까지 유지한다. 잘못된 ID·NaN/Inf는 거부하고 정지한다. 미래 CFD는 step 경계에서 이 입력을 갱신할 수 있지만 현재 항력·셀 매핑·공극률·양방향 교환은 없다. 이것만으로 CFD–DEM 연성이 완료된 것은 아니다. 움직이는 경계에는 향후 경계 속도·접촉점 속도·수명 계약이 별도로 필요하다.

팀에는 `Plugins/DEMPrototype`의 uplugin/Source/Tools/Tests, `Content/DEM`의 Settings·Emitters·Simulation·Boundaries·공유 메시/재질·DEM_Integration을 공유한다. 검증 재현에는 CommonValidation/Before의 이전 솔버 기준본도 필요하다. Binaries/Intermediate는 엔진/컴파일러별로 다시 빌드한다. Runtime DEMPrototype과 Editor DEMPrototypeEditor 모듈을 사용한다. 기존 DEM_Demo/L_Main 및 시작 맵을 교체할 필요가 없다.

## 빌드·생성·검증 명령

에디터의 작업을 저장하고 종료한 뒤 원래 프로젝트 루트의 PowerShell에서 순서대로 실행한다.

```powershell
.\Plugins\DEMPrototype\Tools\CommonDEM.ps1 -Task Build
.\Plugins\DEMPrototype\Tools\CommonDEM.ps1 -Task Create
.\Plugins\DEMPrototype\Tools\CommonDEM.ps1 -Task Verify
.\Plugins\DEMPrototype\Tools\CommonDEM.ps1 -Task BuildCore
.\Plugins\DEMPrototype\Tools\CommonDEM.ps1 -Task Core
```

Create는 기존 에셋/Integration 맵을 덮어쓰지 않는다. Verify는 별도 원래 프로젝트 프로세스에서 로드하고 임시 월드로 시험한다. 시험 가정값은 r=.01m, rho=1000kg/m³, kn=500000N/m, kt=250000N/m, 감쇠비=.2, 마찰=.4, dt=1e-5s다. 실제 분말 측정값이 아니다. 반지름/강성을 바꾸면 dt 안정성도 재검증해야 한다. 사용자 기본 설정이 다르면 생성 과정은 검증용 복사본을 사용한다.
