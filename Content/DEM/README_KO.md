# DEM 협업 편집 안내

Unreal 5.8 프로젝트를 열고 Content/DEM/DEM_Demo 맵을 엽니다. Plugin Content 표시 설정이 필요하지 않습니다.

- BP_DEMSimulation: 더블클릭 → Class Defaults에서 DEM 설정을 변경하고 Compile/Save합니다. 맵에서 DEM Simulation 액터를 선택하면 해당 배치만 변경할 수 있습니다.
- Reset settings: 입자 수, seed, 반지름(m), 밀도, 강성, 감쇠, 마찰, dt. 실행 중 변경했다면 Reset을 눌러 적용합니다. 편집기 초기 배치는 Editor Preview로 표시합니다.
- Appearance: Particle Mesh와 Particle Material로 외형을 변경합니다. 메시 교체는 물리 형상을 변경하지 않습니다. 중심이 원점인 구 메시를 사용하세요. High Resolution이 켜져 있으면 비교용 엔진 구 메시가 우선합니다.
- DEM_Demo: 조명, 카메라, 용기 표시 액터를 일반 맵처럼 편집하고 저장합니다. Play 후 Space 시작/정지, N 한 단계, R 초기화입니다.
- SM_DEMSphere: 편집 가능한 공유 구 메시입니다. 입자별 Actor를 만들지 않습니다.

주의: 용기 메시를 옮기거나 크기를 바꾸는 것으로 해석 경계가 바뀌지는 않습니다. 현재 해석 경계는 C++ 코어의 사각 용기입니다. 실행 Reset 시 용기 표시는 해석 설정에 맞춰 재배치됩니다. 이 맵은 원점 기준 독립 시험 장면이며 기존 설비와의 충돌 연동 기능은 없습니다.

협업 저장소에는 Content/DEM 전체와 Plugins/DEMPrototype의 소스 및 .uplugin을 함께 포함하세요. C++ 해석 코어는 검증된 플러그인에 유지되므로 Content 폴더만 전달하면 동작하지 않습니다. Binaries, Intermediate, DerivedDataCache는 공유할 필요가 없으며 팀원 환경에서 프로젝트를 빌드합니다. .uasset/.umap은 바이너리 파일이므로 동일 파일 동시 편집을 피하거나 Git LFS 잠금을 사용하세요.

기존 DEM_Sandbox 및 검증 결과는 원본으로 보존합니다. 기존 L_Main과 기본 시작 맵은 변경하지 않습니다.
