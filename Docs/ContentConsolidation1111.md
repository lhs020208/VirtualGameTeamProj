# 1111 콘텐츠 통합 (2026-11-25)

사용 레벨은 `/Game/ProcessPlant_20261111_LowPoly/L_ProcessPlant_LowPoly_1111` 하나로 통합했다.

- Assets: 기존 설비 메시·머티리얼 유지.
- Materials: 이전 1029의 공용 단면/단순 머티리얼 이동.
- Particles: 입자 구 메시 및 M_BulkFlow 이동.
- UI: 현재 WBP_ProcessUIV2 이동. 표시 토글 기능 유지.
- 기본 시작 맵과 C++ 하드코딩 에셋 경로를 새 위치로 수정.
- 1111에 CUDA_PTX_GRAPH 계산 설정 적용, DemoRealtime 비활성화.
- CUDA·데모·UI 테스트 맵과 미사용 UI 시제품은 Content에서 제외.
- 원본 SourceAssets 및 물리 계산 코드는 유지. 이번 작업은 콘텐츠 통합이다.

제외 파일은 자동 승인 검토의 삭제 명령 차단으로 Saved/Consolidate1111/RemovedContent에 복구용 보관했다. Content 밖이므로 에디터 에셋으로 로드되지 않으며 Git에서는 삭제로 표시된다.

검증 자료: Saved/Consolidate1111/{migration,verification,asset_audit}.json 및 Saved/Consolidate1111Build.log.

검증 결과: UE 빌드 성공, ProcessUI 자동 검사 75개 성공/실패 0개, 2,293개 에셋 누락 참조 0개·외부 Content 참조 0개. 유일한 레벨과 CUDA_PTX_GRAPH 설정 재로딩 확인.
