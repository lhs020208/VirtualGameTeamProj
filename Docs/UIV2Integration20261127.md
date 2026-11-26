# UI_v2_apply 최신 변경 병합 (2026-11-27)

- 대상: `PowderSimulationOptimization`의 `bbac26c3`에 `origin/UI_v2_apply`의 `29a33644` 병합.
- UI에서 공급 A~F 자동 투입, 레시피 순서, 운전 중 개별 압력 및 RPM 조절, 웹 조회·제어 기능을 통합했다.
- 현재 레벨 `/Game/ProcessPlant_20261111_LowPoly/L_ProcessPlant_LowPoly_1111`과 1125 최신 모델, OpenCL 선택, A/B 각각 10,000개 및 물리 입력 데이터를 유지했다.
- 3D 보기의 프레임 표시와 설비 전체/전면 개방/숨김 기능, 최신 혼합기 형상 집계와 회전 부품 태그를 유지했다. 표시 전환은 물리 벽을 바꾸지 않는다.
- 정리한 1111 에셋 경로를 유지하며 삭제된 CUDA·데모 레벨을 복구하지 않았다.

## 충돌 및 계산 경로 호환성

`PowderPlantTest.cpp`는 기존 입자 표시 헬퍼와 새 개별 공급압 변환을 모두 유지했다. 모듈 의존성은 기존 Messaging 계열과 새 HTTP 모듈을 함께 유지했다.

새 UI의 공급기 6개 제어에 맞춰 선택 가능한 CUDA 경로도 공급 브레이크 6개를 전달하도록 확장했다. OpenCL에서 생성하는 CUDA 헤더와 PTX를 재생성하고 출처 해시를 갱신했다. 레벨의 기본 계산 경로는 계속 OpenCL이다.

## 검증

- Unreal Editor Win64 Development 빌드 성공.
- Unreal `ProcessUI+ProcessWeb` 자동화 85개 통과, 실패 0, 정상 종료 코드 0. 최초의 넓은 `Process` 검색은 엔진 자체 SSAM 렌더링 검사까지 포함하여 NullRHI에서 실패했으므로 프로젝트 검사 범위를 명시해 재실행했다.
- 압력·밸브 회귀 검사: 128개 CPU 조건 및 GPU 항력 방향/30회 필드 갱신 통과.
- 6개 공급 브레이크의 OpenCL/CUDA PTX/CUDA PTX Graph 위치·속도·공기장 비교, 토글·재설정·초기화 검사 통과.
- 최신 모델 OpenCL 검사: 20,000개 초기 벽 겹침 0, 혼합기 표면 60,084개 및 관 경로 837개가 원본 삼각형 기준과 일치. 36입자 국소 시험에서 입구 12개 통과, 내부 유지, 회전 시작/정지 통과.
- 웹 단위 테스트 45개 및 웹 빌드 통과. Edge headless에서 조회·수동 제어·자동 배치·압력 변경·오류/재연결의 5개 브라우저 검사 묶음 통과. 웹 브라우저 검사는 CPU 모의 상태를 사용하며 실제 설비 성능 검증이 아니다.

빌드/자동화 로그와 로컬 검사 결과는 `Saved/UIUpdate1127*`, 웹 화면 검사는 `Saved/ProcessWeb*BrowserChecks`에 저장한다. 장시간 전체 공정 또는 실물 물성 보정을 새로 검증한 것은 아니다.

## 웹 기능

기존 언리얼 UI는 그대로 사용한다. 웹 기능은 실행 인수 `-ProcessWebState` 또는 `-ProcessWebControl`을 명시해야 활성화된다. 이번 병합은 실제 웹 서버를 상시 실행하거나 제어권을 웹으로 옮기지 않는다. 연결 및 사용법은 `Tools/ProcessWeb/README.md`를 따른다.
