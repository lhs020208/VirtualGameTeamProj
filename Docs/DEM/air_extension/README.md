# 공기저항 1차 확장 — 구현·검증 결과

2026-10-28. `분체역학_공기저항1차_작업계획.md`의 01~07 완료. 현재 브랜치 PowderSimulationOptimization. 커밋하지 않았다.

## 구현 범위

GPU 입자에 **주어진 균일 공기 속도의 단방향 구형 항력 + 정수압 부력**을 추가했다. OpenCL을 유지하고 기존 predict 커널에 융합하여 추가 dispatch/입자 전송을 만들지 않았다. 기본 OFF이며 저장된 레벨·데이터에셋·입자 수·dt·반복 횟수·목표 배속을 변경하지 않았다.

공기 밀도, 동점도가 아닌 **점성계수(Pa·s)**, 믹서 로컬 좌표계 속도(m/s)를 설정한다. source_diameter=0은 가상 구를 뜻하며, 실제 미세분말의 항력으로 해석하지 않는다. source_diameter가 주어진 경우 계산 지름과 다르면 미검증 축약을 거부한다. GPU에서는 질량 물성이 명시된 Binary Materials 모드가 필요하다.

현재 계산 구의 실제 체적×밀도를 질량으로 사용한다. 현재 지름 약 10~12.7mm에 적용되는 항력이며, 미세입자의 거동을 대신하는 보정 모델은 아니다.

## 작업별 완료와 증거

| 작업 | 상태 | 변경/검증 |
|---|---|---|
| 01 적용 계약 | 완료 | air.hpp, granular.hpp. 기본 OFF, 유한값·단위·범위·지름 대응 검증 |
| 02 CPU 기준 | 완료 | air_tests.cpp. Stokes 극한/고 Re 힘식, 정지·감쇠·방향, 부력, 종말 침강 힘 평형, 시간간격 수렴 |
| 03 GPU 연결 | 완료 | granular.cpp / granular_kernels.inc. 두 물성 및 후보 목록 ON/OFF 경로 CPU 비교, 안전 거부 |
| 04 에디터 연결 | 완료 | PowderGranularMixer.h/.cpp, EmitterIntegrationTests.cpp. 설정·Reset·상태 안내·air_model.json, UHT/Editor 빌드 성공, 자동 시험 통과 |
| 05 회귀·물리 | 완료 | 기존 granular 7개 + 신규 2개 전체 통과. 추가한 종말 속도/GPU 시간간격 수렴을 포함한 최종 신규 2개 재실행 통과 |
| 06 5만 개 측정 | 완료 | validate_prescribed_air.py, granular_diagnose.cpp. OFF/still/wind 각각 2회, 순서 역전, 같은 초기 SHA·설정, 최종 기하 감사 모두 통과 |
| 07 최종 검토 | 완료 | git diff --check 통과, 기존 Content/Config 변경 없음. 현재 실제 맵의 별도 headless 실행으로 ON 설정 전달 및 결과 기록·기하 감사 통과 |

증거: `core_tests.log`, `final_air_tests.log`, `unreal_build.log`, `unreal_tests.log`, `performance.json`, `unreal_smoke.json`. 실제 재료 실험 정확도, 장시간 통계적 수렴, 새 기능 ON 상태의 30FPS 유지 시험은 **미실행**이다.

## 수식과 적용 한계

Re = rho_air × diameter × |u_air − v| / mu.

Cd·Re = 24(1 + 0.15 Re^0.687) (Re ≤ 1000), 그 이상은 0.44 Re. beta = 3 mu (Cd·Re)/(16 rho_particle radius²). Re=0에서 0으로 나누지 않고 Stokes 극한을 보존한다.

v_new = [v_old + dt × {g(1 − rho_air/rho_particle) + beta u_air}] / (1 + dt beta).

beta는 이전 속도에서 계산하는 1차 반암시적 근사다. 시간간격 수렴 검사가 필요하며 무조건 정확한 적분이 아니다. beta·dt > 0.25, Re > 200000, 상대속도 > 100m/s 또는 비유한값이면 GPU 안전 비트 32로 거부하고 Reset을 요구한다. 상한은 구현의 보수적인 적용 계약이지 모든 온도·압력에서의 정확도 보장이 아니다. 공기 ON 시 벽 마찰의 지지 충격량은 항력·부력을 반영한 예측 속도로 추정하여 기존 g·dt를 추가로 중복하지 않는다. 여전히 위치 기반 접촉 근사이며 접촉력 측정 인증이 아니다.

공기 속도장은 벽을 따라 풀리지 않는다. 밀집 입자 차폐, 유한 공극률, 입자가 공기에 되돌리는 힘, 압력손실, 진공/압축성 유동, 난류, 응집·벽부착, 입도분포, 비구형 형상은 이번 구현에 없다. **5만 개 밀집 장면의 공기 ON 시험은 계산 비용/수치 안전 확인용이며 고농도 공정 예측이 아니다.**

공식 구현 참고: [OpenFOAM Schiller–Naumann 구형 항력](https://cpp.openfoam.org/v7/SchillerNaumannDragForce_8C_source.html). 식을 확인한 참고 자료이며 외부 해석기를 호출하거나 도입하지 않았다.

## 현재 5만 개 성능

동일 초기 SHA `9547042760b376c83d3820068abe4e52b4093235062f4476a97c3fbb1cb8e29d`, dt=0.000375s, 16회 접촉, 배치 32, 동일 회전 시작, 50ms 간격 관측. 각 4.008초 물리 시간, 모드당 2회. 사용자 에디터를 열어 둔 **계산 전용** 비교다.

| 모드 | 실제 시간 1초당 시뮬레이션 진행 중앙값 | OFF 대비 계산 시간 |
|---|---:|---:|
| OFF | 0.3593s | 기준 |
| 정지 공기 | 0.3621s | -0.8% (측정 변동 범위, 가속 효과 주장 불가) |
| 로컬 X 방향 2m/s 공기 | 0.3426s | +4.9% |

표본이 짧고 궤도/접촉 수 자체가 달라지므로 위 차이는 항력 커널만의 독립 비용이 아니다. 이 결과를 화면 포함 0.34배속 보장으로 해석하지 않는다. 이전 30FPS 0.2배속과 직접 비교할 수 없으며 ON 상태의 화면 포함 시험이 별도로 필요하다.

비용 비교 중 최고 관측 온도 59°C, 최고 전력 130.11W였다.

6회 모두 50,000개/약 40.837923kg/그룹 ID 보존, 최종 입자 겹침 ≤ 작은 반지름의 10%, 벽·회전체 침범 ≤ 1e-4m 기준 통과. 모든 시각의 연속 검증이나 실제 분말 정확도 증명이 아니다. 자체 시험은 GPU 온도 75°C/전력 155W/계측 실패 시 중단하도록 구성했다.

현재 맵 별도 headless smoke: 공기 ON, X=2m/s, 물리 2.019375초, 결과에 설정 반영 확인. 초기/최종 기하 감사 2개 통과(최대 겹침 비율 약 5.835%). 렌더링/FPS 시험이 아니다.

## 사용

1. 현재 에디터에서 작업을 저장하고 재시작하여 새 C++ 속성을 로드한다. 기존 사용자 에디터는 종료하지 않았다.
2. L_MX301_BinaryMaterials의 PowderGranularMixer 액터에서 `Granular > Air > Enable Prescribed Air`를 켠다. Binary Materials 및 기존 emitters를 사용한다.
3. Air Density Kg M3 / Air Viscosity Pa S / Air Velocity Meters Per Second를 지정한다. 속도는 믹서 로컬 축의 m/s이다.
4. 실행 중 값을 바꿨다면 Reset Simulation으로 적용한다. 밀집 공정의 실제 유동을 예측하는 설정은 아니다.
5. 실행 결과 폴더의 air_model.json에서 적용된 모델·수치를 확인한다.

별도 실행 옵션: `-GrainPrescribedAir -GrainAirX=2 -GrainAirY=0 -GrainAirZ=0`. 에셋을 저장하지 않는 실행 시 옵션이다.

## 재현과 복구 내역

- Core: CMake `Intermediate/PowderCore`, Release, `air_tests`, `granular_air_tests` 및 기존 granular 대상 빌드. CTest `-R 'physics_.*air|granular'`.
- 계산 비용: `python Scripts/validate_prescribed_air.py`. 현재 고정 initial.bin이 필요하며 기존 실행 폴더를 덮어쓰지 않는다.
- 실제 맵 연결: `python Scripts/validate_air_unreal.py`. 별도 UnrealEditor-Cmd 프로세스와 고유 출력 폴더를 사용한다.
- Editor build: Build.bat VirtualGameTeamProjEditor Win64 Development, 프로젝트 지정, `-ModuleWithSuffix=VirtualGameTeamProj,1029 -ModuleWithSuffix=DEMPrototype,1029`. 실행 중 사용자 DLL을 덮어쓰지 않도록 두 모듈의 suffix를 일치시켰다.
- 초기 GPU 중성부력 시험의 엄격한 `==0` 판정은 부동소수점 잔차 때문에 실패하여 허용오차 1e-7m/s로 수정했다. 최종 통과.
- 별도 UE smoke 첫 실행은 서로 다른 hot-reload suffix의 플러그인 중복 로딩으로 종료 코드 3. 두 모듈을 같은 suffix로 재빌드하고 새 프로세스 재시험하여 통과. 실패 로그는 Saved/PrescribedAirUnreal/engine.log에 보존.
- 빌드 중 기존 시각화 코드의 double→float 경고가 있었으며 이번 공기 모델 컴파일 오류는 없다.

## 변경 파일

Physics/PowderCore: include/powder/air.hpp, include/powder/granular.hpp, src/granular.cpp, src/granular_kernels.inc, tests/air_tests.cpp, tests/granular_air_tests.cpp, app/granular_diagnose.cpp, CMakeLists.txt.

Source/VirtualGameTeamProj: PowderGranularMixer.h, PowderGranularMixer.cpp, EmitterIntegrationTests.cpp.

Scripts: validate_prescribed_air.py, validate_air_unreal.py. Docs/DEM: 이번 계획서 및 air_extension/ 증거/설명.

다음 개발은 실제/가상 입도와 축약 모델 확정, 접촉 보정 시험, 응집·벽부착, 보존적인 공기 격자 전달 및 고농도 양방향 유동 순서다. 이번 7개 완료가 전체 분체역학 목표 완료를 뜻하지 않는다.
