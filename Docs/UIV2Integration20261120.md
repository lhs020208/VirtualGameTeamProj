# UI v2 통합 검증 (2026-11-20)

- 대상: PowderSimulationOptimization(d68de0c5) + origin/UI_v2_apply(b8dbb943).
- 공정도/3D 관찰, 장비 수동 제어, 압력·레시피, 자동 배치와 복구 UI를 통합했다.
- 기존 CUDA 백엔드, 기본 CUDA1118 레벨, 별도 DemoRealtime 시연 경로를 유지했다. CUDA 레벨에 명시적 UI 액터 연결을 추가했다.
- UI의 공급 제동 3채널과 공기 상태 조회를 CUDA/PTX/Graph 경로에도 연결했다. 제동 변경·초기화 시 캡처 그래프를 무효화하며 PTX를 재생성했다.
- 예전에 삭제한 설비 에셋은 복원하지 않았다. UI 브랜치의 위젯과 UI 테스트 맵은 포함했다.

## 검증

- UE 5.8 Win64 Development Editor 빌드 성공.
- ProcessUI 자동 검사 75개 성공, 실패 0개.
- CUDA UI 회귀 검사: OpenCL 대비 CUDA PTX/Graph 제동 활성/변경/해제, 위치·속도, 공기 조회, 초기화 검증 성공.
- 압력 검사: CPU 128개 조건 및 GPU 항력 방향·30회 경계조건 갱신 성공.
- CUDA1118 실제 PIE에서 새 UI 표시, 실시간 밸브/질량/RPM 갱신 및 CUDA Driver frozen PTX 장치 연결 확인. 검증 후 PIE를 종료하고 에디터를 편집 모드로 유지했다.
- 장시간 전체 공정 및 20만 입자 성능 재측정은 수행하지 않았다. 기존 물성의 현실 정확도 보정을 의미하지 않는다.

## 사용

L_ProcessPlant_CUDA_1118에서 Play하면 UI가 열린다. 자동 배치 시작 전 레시피 또는 개별 설정 및 양수 혼합 시간을 적용해야 한다. 시연 전용 DemoRealtime 레벨은 기존 시연 패널을 사용한다.

로컬 검증 로그: Saved/UIIntegrationBuild3.log, Saved/UIIntegrationAutomationConsole.log, Saved/UIIntegrationCUDAControls.log, Saved/UIIntegrationPressure.log.
