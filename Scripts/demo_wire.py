"""One-time opt-in integration; does not change existing level defaults."""
import pathlib
root=pathlib.Path(__file__).resolve().parents[1]
p=root/'Source/VirtualGameTeamProj/PowderPlantTest.cpp'
s=p.read_text(encoding='utf-8')
if '#include "PowderPlantDemo.inl"' in s:
    print('Demo integration already present');raise SystemExit(0)
def replace(old,new):
    global s
    assert old in s,old[:100]
    s=s.replace(old,new,1)
replace('#include "powder/plant_pressure.hpp"','#include "powder/plant_pressure.hpp"\n#include "powder/demo_plant.hpp"')
replace('#include "PowderPlantRuntime.inl"','#include "PowderPlantRuntime.inl"\n#include "PowderPlantDemo.inl"')
replace('Generation=0;CurrentPhase=0;','Generation=0;CurrentPhase=0;DemoAutoStage=-1;')
replace('if(!std::isfinite(FixedDt)||FixedDt<1e-6||FixedDt>2.5e-5||ContactIterations<4||ContactIterations>32||!std::isfinite(StageTimeoutSeconds)||StageTimeoutSeconds<=0)', 'if(!DemoRealtime&&(!std::isfinite(FixedDt)||FixedDt<1e-6||FixedDt>2.5e-5||ContactIterations<4||ContactIterations>32||!std::isfinite(StageTimeoutSeconds)||StageTimeoutSeconds<=0))')
replace('C.dt=float(FixedDt);C.iterations=unsigned(ContactIterations);','C.dt=DemoRealtime?1.f/120:float(FixedDt);C.iterations=DemoRealtime?1u:unsigned(ContactIterations);')
replace('PressureStage,PressureManualOverride);RunLogPath','PressureStage,PressureManualOverride,DemoRealtime);RunLogPath')
replace('void APowderPlantTest::BeginPlay(){Super::BeginPlay();','void APowderPlantTest::BeginPlay(){Super::BeginPlay();if(DemoRealtime){FParse::Value(FCommandLine::Get(),TEXT("DemoBenchmarkSeconds="),DemoTestSeconds);FParse::Value(FCommandLine::Get(),TEXT("DemoBenchmarkOutput="),DemoOutput);if(DemoTestSeconds>0){Running=false;DemoFrames.Empty();DemoWarmFrames=0;}}')
replace('void APowderPlantTest::Tick(float D){Super::Tick(D);','void APowderPlantTest::Tick(float D){Super::Tick(D);if(DemoRealtime)TickDemo(D);')
replace('if(LastShow>.1){Show();LastShow=0;}','if(DemoRealtime||LastShow>.1){Show();LastShow=0;}')
for name,signature in [('ToggleSupplier','int32 Index'),('ToggleWeighingHopper',''),('ToggleReceiver',''),('ToggleMixer','')]:
    replace('void APowderPlantTest::'+name+'('+signature+'){','void APowderPlantTest::'+name+'('+signature+'){DemoAutoCycle=false;')
replace('TEXT("시뮬레이션 정보 · 배속 제한 없음")','DemoRealtime?TEXT("시연용 · 목표 1배속 · 표시 20,000 / 계산 2,500"):TEXT("시뮬레이션 정보 · 배속 제한 없음")')
replace('return SNew(SBox).WidthOverride(360)[Panel];','if(DemoRealtime)Panel->AddSlot().AutoHeight().Padding(0,8)[SNew(SButton).OnClicked_Lambda([Weak](){if(Weak.IsValid()){Weak->DemoAutoCycle=!Weak->DemoAutoCycle;Weak->DemoAutoStage=-1;}return FReply::Handled();})[SNew(STextBlock).Text_Lambda([Weak](){return FText::FromString(Weak.IsValid()&&Weak->DemoAutoCycle?TEXT("자동 시연 중 · 수동으로 전환"):TEXT("자동 시연 시작"));})]];\n return SNew(SBox).WidthOverride(360)[Panel];')
replace('TEXT("1~4: 장치 확대 | Space: 일시정지\\nR: 초기화 | O: 입자 저장\\n관로 압력 근사 + GPU 입자 시험 · 미보정")','DemoRealtime?TEXT("1~4: 장치 확대 | Space: 일시정지\\nR: 초기화 | O: 입자 저장\\n시연용 근사 모델 · 정밀 분체 해석 아님"):TEXT("1~4: 장치 확대 | Space: 일시정지\\nR: 초기화 | O: 입자 저장\\n관로 압력 근사 + GPU 입자 시험 · 미보정")')
p.write_text(s,encoding='utf-8')
