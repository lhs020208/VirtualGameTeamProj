# 최적화 14 — GPU 설계·메모리 계약과 최소 실행

상태: 제한된 GPU 기반 구현·검증 완료. 전체 DEM 포팅 또는 목표 배속 달성을 뜻하지 않는다.

`gpu.hpp`, `gpu_opencl.cpp`, `gpu_kernels.inc`에 GPU 상주 위치/속도/회전/재질 상태, 명시적 업로드·다운로드, 실패 전 커밋 차단을 구현했다. Windows 시스템 OpenCL.dll을 동적으로 읽으며 CUDA SDK 설치를 요구하지 않는다. Khronos 헤더는 고정 커밋과 라이선스 헤더를 포함해 `third_party/OpenCL`에 보관했다. FP64 미지원 장치는 거부한다. fast-math/FMA 축약을 켜지 않는다.

초기 실측 GPU 총 메모리 16,380 MiB, 가용 12,497 MiB. 장치 단일 할당 한도 4,293,787,648 byte. 코드의 예산은 장치 총량의 절반 이하이며 실제 할당 실패도 오류로 처리한다. 가용량은 에디터/다른 앱에 따라 변한다. 코드 예산은 드라이버 내부 메모리를 포함하지 않아 추가 여유가 필요하다.

최종 16 포함 메모리식: 상태 424N byte, 공간 정렬 16×next_power_of_two(N), 접촉 이력 144×N×slots, 고정 여유 4,096 byte. 무접촉 전용은 상태 256N. 목표 N=1,591,550, slots=32에서는 8,042,238,128 byte(약 7.49 GiB). clone/replace는 일시적으로 별도 상태를 만들어 최고 메모리가 증가하므로 대량 장면에 무료 체크포인트처럼 사용하면 안 된다.

검증: 257개 다분산 구, 중력+단일 외력, dt=1e-5s, 1,000스텝에서 CPU 최대 차이 0. 중복 ID, 음수 dt, 부족한 메모리 예산 거부 확인. `optimization14/final/result.csv`에 업로드/연산/다운로드의 host 시간과 GPU event 시간을 각각 기록했다. 초기 `free_validation`은 개발 중 결과이며 최종 근거는 `final`이다.

한도: 2백만 구, 1..128 접촉 슬롯, 장치 메모리 예산. API/장치 실패로 상태가 불확실하면 이후 진행/스냅샷을 차단하고 reset을 요구한다. 전원/드라이버 장애를 실제 주입한 복구 시험은 하지 않았다.

API 근거: [OpenCL FP64](https://registry.khronos.org/OpenCL/specs/unified/refpages/man/html/cl_khr_fp64.html), [장치 메모리 조회](https://registry.khronos.org/OpenCL/specs/unified/refpages/man/html/clGetDeviceInfo.html). 이 문서는 구현 API 근거이며 물리 정확도 보증은 자체 검증 범위에 한정된다.

검증 환경: 2026-10-22, Windows, RTX 4060 Ti 16GB, OpenCL FP64, MSVC Release /fp:strict, UE 5.8 Development Editor. 브랜치 PowderSimulationOptimization. 이번 요청에서 커밋/푸시는 하지 않았다.

Core 전체 41개 테스트 통과 후 변경된 GPU/인계 테스트를 재검증했다. Unreal 실제 RHI 활성화 환경의 16개 테스트 모두 통과했다. 재현은 프로젝트에서 `python Scripts/run_optimization14_18_validation.py --output Saved/Optimization14_18_new --full-target`. 이 스크립트는 Core/GPU 시험이며 Unreal 실행 결과는 별도다. 원본 로그와 코드 SHA-256은 `optimization14_18/`에 보관한다.
