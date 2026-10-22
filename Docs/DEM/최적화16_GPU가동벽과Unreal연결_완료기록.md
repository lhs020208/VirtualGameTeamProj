# 최적화 16 — GPU 가동 벽·혼합·Unreal 연결

상태: 기본도형/구형 지원 경로 구현 및 기능·수치 검증 완료. 완전한 뷰포트 GPU 프레임 비용과 대량 렌더 포함 이득은 미검증이며 0.1배속 승인은 없다.

정적 평면, 검증한 상자 메시, 1개 가동 상자의 처방 회전/이동·벽 접점 속도·반력 토크·외부 일을 구현했다. 총 벽 8개까지다. UE 기본 Cube는 12개보다 많은 삼각형을 사용하므로 삼각형 수로 단정하지 않고 닫힌 셸/각 면의 위치·방향·총면적을 검사한다. 임의 메시를 경계 상자로 바꾸지 않는다. 얇은 벽의 스텝 사이 관통도 보수적 선분/회전 여유 검사로 거부한다.

`gpu_clump.cpp` 어댑터를 `ClumpWorker`에 연결했다. GPU 상태는 워커가 소유하며 명령 장벽, 실행/일시정지/한 스텝/reset/독립 체크포인트를 지원한다. 화면 스냅샷과 물리시각 혼합 샘플에서만 전체 상태를 읽는다. 측정은 렌더 표본을 재사용하지 않는다. 체크포인트 복제는 명시적이며 비용이 크다.

Unreal 사용: PowderClumpTest의 **Prefer GPU**를 켜고 Reset. 지원하지 않는 형상/물성은 **Backend Status**에 CPU fallback 사유가 표시된다. 기본은 CPU다. 기본 서로 다른 원료 계수는 유지한다. 공통 계수를 원하는 경우 사용자가 **Uniform Damping Friction**을 명시하고 감쇠·마찰 값을 정하며, Powder/Mixed/Inclusion Stiffness도 동일하게 설정해야 현재 GPU 경로가 선택된다. 이 설정은 CPU에도 동일하게 적용되고 내보내기에 기록된다. 변경 시 Reset이 필요하다.

검증: 가동 벽 1,000스텝 CPU/GPU 상태차 2.26815e-16, 토크차 1.73269e-17, 벽 일 1.65312e-9J 일치. 정적 평면/상자 모서리, 얇은 벽 관통 거부, 활성 이력 clone, 워커 제어 검증. 별도로 32개 혼합 장면의 물리시각 샘플 질량·지수·궤적을 비교했다. `optimization16/result.csv` 및 `optimization14_18/unreal_final.json` 참조.

Unreal RHI 활성화 자동화에서 GPU/CPU 동일 장면을 실행하고 실제 ISM 인스턴스 갱신 시간, 워커와 스냅샷 비용을 기록했다. 자동화 월드는 대량 뷰포트 프레임 시간 측정이 아니므로 이를 화면 렌더 전체 시간으로 표현하지 않는다. 작은 32개 장면에서는 GPU 비용이 더 컸으므로 CPU 기본값을 유지했다. 단독 비용 재측정은 `optimization14_18/unreal_cost.json`.

첫 실행은 엔진 Zen DDC 스레드에서 해석기 진입 전 충돌했다. 파일 캐시 그래프 `-ddc=InstalledNoZenLocalFallback`으로 세션만 변경해 검증했으며 프로젝트 영구 캐시 설정을 수정하지 않았다.

검증 환경: 2026-10-22, Windows, RTX 4060 Ti 16GB, OpenCL FP64, MSVC Release /fp:strict, UE 5.8 Development Editor. 브랜치 PowderSimulationOptimization. 이번 요청에서 커밋/푸시는 하지 않았다.

Core 전체 41개 테스트 통과 후 변경된 GPU/인계 테스트를 재검증했다. Unreal 실제 RHI 활성화 환경의 16개 테스트 모두 통과했다. 재현은 프로젝트에서 `python Scripts/run_optimization14_18_validation.py --output Saved/Optimization14_18_new --full-target`. 이 스크립트는 Core/GPU 시험이며 Unreal 실행 결과는 별도다. 원본 로그와 코드 SHA-256은 `optimization14_18/`에 보관한다.
