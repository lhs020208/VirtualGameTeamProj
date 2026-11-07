# 20261104 설비 이전 결과

## 완료
- 소스: C:/Users/lpet2/OneDrive/바탕 화면/hoper_팀원전달_20261104.zip 내부 hoper.blend. SHA256 C083BE71A47045FF0AC1545E6199CDFD57961131C1F7C0C5574A70EE21C70DBF. 원본 변경 없음.
- 실제 열려 있던 /Game/ProcessPlant_20261103/L_ProcessPlant_ParticleSetup에 적용 및 저장. 레벨 이름은 그대로 유지했다.
- 설비 StaticMeshActor 2254개를 새 FBX 자산으로 교체. 액터 자체, 변환, 머테리얼 오버라이드, GUI 참조, 카메라와 시뮬레이션 설정 보존. 새 FBX 가져오기 결과와 2254개 변환값 대조에서 차이 0.
- 새 ClosedSeal 보조 표시 7개는 기존 동작 게이트와 중복되므로 배치하지 않았다. 밸브 개방 시 막힌 외형이 남는 것을 방지한다.
- 새 하이브리드 충돌과 공간 탐색 데이터를 별도 SourceAssets/ProcessPlant_20261104/Simulation에 생성하고 기존 시험 액터의 DataDirectory를 변경했다. 관 원본 메시·호퍼 패치·회전체·수동 압력 경로 유지. 6 공급기, 83 포트, 7 유동 경로.
- 수취부 전용 충돌 표면이 이번 원본에서는 이미 월드 좌표이므로 이전 버전용 XY 반전/이동을 중복 적용하지 않도록 런타임 생성기를 수정했다. 이전 로컬 좌표 원본도 계속 지원한다.
- A/C 연결 챔버 바닥 경사와 칭량호퍼 대응 접촉면 수정 반영. 임의 형상 근사/물성/공기 모델 변경 없음.

## 검증
- 시작 시 편집 월드 dirty map/content 모두 0. 기존 PIE는 정상 종료. 관찰 CSV와 기존 맵 파일 보존.
- PIE 자동 검증: A/B 각 10000개, A~F 개방/폐쇄에 따라 디스크 표시 변경, 칭량·수취 출구 개폐, 혼합기 회전, 리셋 시간 0 및 20000개 재생성, 재개 확인. play_validation.json 참조. 짧은 기능 검증이며 전체 공정 이송 시험은 아니다.
- GPU six-feed 검사: 여섯 분기를 개별 개방했을 때 해당 분기만 양의 유량, 닫힌 게이트 접촉/열린 게이트 통과 검사 모두 PASS. six_feeds.txt 참조.
- 실제 GUI 클릭으로 A 개방, 칭량/수취 출구 개방, 혼합기 시작, 설비 숨김·전면 개방·프레임 독립 표시 전환 확인. 숨김 직후 파란 호스처럼 보였던 잔상은 몇 초 뒤 사라져 입자만 남았다. 표시 목록에서 누락된 설비 PrimitiveComponent는 없었다.
- GUI 시험은 물리 약 0.5초 범위. 장시간 이송의 걸림/누출/관통 및 혼합 품질은 미검증. 이전 18.3초 관찰은 이전 원본 결과이므로 이번 모델의 결과로 대체하지 않는다.

## 남은 물리적 문제/한계
- BV272_111_Ferrule_neck, 혼합기 충돌 로터는 이전 원본과 동일하다. 이전 관찰의 출구 링 170개 잔류 및 날개-바닥 약60.3mm 간격 문제는 이번 모델 교체만으로 해결됐다고 볼 수 없다.
- 수취부 직전 수평관의 1개 잔류도 개선을 주장하지 않는다. 개별 항력/마찰력 계측 및 최신 모델 장시간 재시험 필요.
- 공급기 C~F의 실제 다입자 이송은 초기 입자0이므로 검사하지 않았다. 개별 게이트 GPU 검증과 구별한다.
- 기존 작은 뷰포트에서 밝은 하늘 위 GUI 글자 대비가 낮은 점, cutaway material/Nanite 사용 경고는 별도 표시 개선 대상이다.

## 변경/복구
- Content/ProcessPlant_20261104/Assets: 새 FBX 자산.
- Content/ProcessPlant_20261103/L_ProcessPlant_ParticleSetup.umap: 기존 액터의 메시 참조와 새 런타임 연결.
- SourceAssets/ProcessPlant_20261104: FBX, 공급기 측정, 시뮬레이션 데이터.
- Scripts/build_plant_runtime.py: 수취부 로컬/월드 좌표 판별.
- Scripts/migrate_plant_20261104.py, export_plant_20261104_fbx.py, check_migrated_plant_20261104.py: 이전 및 검증 재현.
- Docs/ProcessPlant_20261104: 계획, 소스/런타임 목록, 액터별 이전 매핑, 검증 결과.
- 원복: 에디터를 정상 종료하거나 다른 맵으로 이동한 후 Saved/PlantMigration20261104/before.umap을 Content/ProcessPlant_20261103/L_ProcessPlant_ParticleSetup.umap으로 복사. 기존 자산과 20261103 런타임은 그대로 있어 해당 맵 복구로 이전 참조가 복원된다. 새 자산을 삭제할 필요는 없다.
- 기존 미커밋 변경/스테이징은 보존. 커밋/푸시 없음. 최종 에디터는 동일 레벨 편집 모드, PIE 종료 상태.
