# 수정된 hoper_lowpoly 모델 적용 (2026-11-25)

적용 레벨: `/Game/ProcessPlant_20261111_LowPoly/L_ProcessPlant_LowPoly_1111`

원본: `C:/Users/lpet2/OneDrive/바탕 화면/hoper_lowpoly.blend`

SHA-256: `43acdf471871cbf15b98001b7173773fd0bbb693f6b8b938d30fb5c1e8b861c9`

## 반영 내용

- 수정본의 공정 모델과 배경을 현재 레벨에 배치했다. 기존 제어 액터의 이름·경로 및 UI v2 연결은 유지했다.
- 혼합기 길이 1.5 m, 원형 하부 반경 0.3225 m, 축 위치 `(3, 0, 0.9)` m를 실제 월드 형상에서 확인했다. Blender의 일부 과거 피벗 메타데이터 대신 실제 변환을 사용했다.
- 회전 계층의 메시 47개를 `PlantMixerRotor` 태그로 연결했다. 16개 패들, 암·허브·축·커플링·회전 센서 표식이 함께 움직이며, 고정 씰은 `PlantMixerStatic`으로 구분한다.
- 공정 표시 대상 2,268개 중 지지 프레임 387개를 별도 분류했다. 전체/전면 개방/설비 숨김과 독립 프레임 표시 기능을 유지했다.
- 배경 메시 247개는 `PlantBackgroundDisplayOnly`로 구분한다. `NoCollision` 프로필, 오버랩 비활성화, 공정 가시성 목록 및 해석 데이터 제외를 적용했다. 배경 조명기구의 형상은 가져오되 Blender 조명 6개는 추가하지 않았다.
- 모든 배치 메시에는 기존 **Unlit + 색상 × 단순 면 방향 명암** 머티리얼을 적용했다. 반사·텍스처·실시간 조명 기반 셰이딩을 사용하지 않고 그림자도 생성하지 않는다.
- 현재 레벨에 남아 있던 CUDA 선택 값을 **OpenCL**로 변경했다. CUDA 레벨이나 기능은 추가하지 않았다.
- A/B 각각 10,000개, 입자 반지름 2 mm, 밀도 1,200 kg/m³ 및 기존 운전 조건을 유지했다. 시작 파일은 이전 `test_charge.csv`와 바이트 단위로 같다.

## 충돌과 집계

`SourceAssets/PlantRevision1125/Simulation`을 사용한다. 기존 하이브리드 구성처럼 호퍼 평면은 원본에 맞춘 얇은 OBB/클리핑 패치, 관은 상세 삼각형으로 계산한다. 새 혼합기의 용기와 오목한 패들은 원본 충돌 삼각형을 유지하여 형상 손실을 피한다. 위치에 따른 기존 벽 후보 인덱스도 재생성했다.

실제 사용되는 충돌 메시 중 변경된 것은 `MX-301_Collision_Vessel`, `MX-301_Collision_Rotor` 두 개다. 다른 80개 충돌 메시와 837개 공기 유동 셀은 기존 데이터와 동일함을 확인했다. 배경은 이 목록에 없다. 가시성 변경은 셰이더/표시만 바꾸며 해석 벽을 제거하지 않는다.

혼합기 입자 도착 집계는 이전 절반 크기의 고정 좌표 대신 현재 BVH에서 내벽·바닥·뚜껑을 측정하도록 변경했다. 기존 타원 하부와 수정본의 원형 하부를 검사하며, 맞지 않는 형상에는 집계 오류를 표시한다. 이는 질량 집계의 변경이며 접촉 해석식을 바꾸지 않는다.

## 검증

- Unreal Editor 빌드 성공.
- 기존/새 혼합기 질량 집계 자동화 검사 2건 통과.
- OpenCL 입자 20,000개의 초기 벽 겹침 0, 초기 진행 후 개수 유지·유한 좌표 확인.
- 혼합기 충돌 표본 60,084건(세 회전각)의 보정 위치가 원본 삼각형 기준과 일치했다. 관 경로 837개 검사도 일치했다.
- 별도 36입자 국소 시험: 입구의 12개 전부 통과, 용기 내부 유지, 15 RPM 회전 및 정지 확인. 국소 계산 시간 약 0.15136 s.
- 레벨 재시작 후 OpenCL·20,000개·충돌 비활성 프로필 유지 확인.
- 표시 모드 3개 × 프레임 상태 2개 검사에서 배경 표시 유지 및 물리 파일 불변 확인.
- 실제 UI의 혼합 시작·정지 호출 및 47개 부품의 공통 회전 검사는 `play_validation.json`에 기록한다.

이 검증은 모델 교체와 제어 연결의 회귀 검사다. 전체 공정의 장시간 이송, 혼합 품질 또는 실제 장비 물성의 보정을 검증한 결과는 아니다.

## 재생성

1. Blender 백그라운드에서 원본을 열고 `Scripts/export_plant_revision1125.py` 실행. 원본은 저장하지 않는다.
2. `python Scripts/build_plant_revision1125.py`로 물리 데이터 생성.
3. 기존 레벨에서 `Scripts/import_plant_revision1125.py` 실행 후 완료를 기다린다.
4. `Scripts/setup_plant_revision1125.py` 실행. 기존 시각 모델만 교체하고 제어 액터는 보존한다.
5. `Scripts/verify_plant_revision1125.py`, `Scripts/check_plant_revision1125_play.py` 실행.

물리 회귀 실행 파일: `plant_revision1125_test <Simulation 경로> <Intermediate/Plant1125/Physics 경로> <결과.json>`.

상세 근거는 `visual_source.json`, `physics_provenance.json`, `level_integration.json`, `opencl_validation.json`, `play_validation.json`에 있다.
