#include "PlantTransferTelemetry.h"
#include "ProcessUIStateAdapter.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include <limits>
#if WITH_DEV_AUTOMATION_TESTS
namespace
{
struct FTransferFixture
{
    powder::GranularConfig C;
    std::vector<powder::PlantRoute> Routes;
    std::vector<powder::Grain> P{{0,0,1.1f,0},{.05f,0,1.1f,0},{3,0,2.5f,0},{-1.2f,1.4f,2.7f,0}};
    FTransferFixture(const TCHAR* Directory=TEXT("SourceAssets/ProcessPlantPhysics"))
    {
        auto Root=FPaths::ProjectDir()/Directory;
        C.plant=true;C.radius=.002f;C.count=unsigned(P.size());
        C.plant_data=powder::read_plant_data(TCHAR_TO_UTF8(*(Root/TEXT("runtime.plant"))));
        Routes=powder::read_plant_routes(TCHAR_TO_UTF8(*(Root/TEXT("runtime.routes"))));
    }
};
FPlantSimulationSnapshot Read(const FPlantTransferTelemetry& T){FPlantSimulationSnapshot S;T.Publish(S);return S;}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlantMassGeometryTest,"ProcessUI.Transfer.GeometryAndMass",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlantMassGeometryTest::RunTest(const FString&)
{
    FTransferFixture F;FPlantTransferTelemetry T(F.C,F.Routes,{});T.Observe(F.P,0,false,false);auto S=Read(T);
    TestTrue(TEXT("model sphere mass from actual solver radius and plant density"),FMath::Abs(T.ParticleMassKg-.00004021238596594936)<1e-10);
    TestTrue(TEXT("current WH uses position, not material type (all same species)"),FMath::Abs(S.WeighingMass.Kg-2*T.ParticleMassKg)<1e-12);
    TestTrue(TEXT("receiver mass independent of original supplier label"),FMath::Abs(S.ReceiverMass.Kg-T.ParticleMassKg)<1e-12);
    TestTrue(TEXT("SI kg and explicit provenance"),S.WeighingMass.Unit==TEXT("kg")&&S.WeighingMass.Source==TEXT("시뮬레이션 계산값")&&S.WeighingMass.UpdatedUtc.GetTicks()>0);
    TestTrue(TEXT("gate plane belongs to WH only"),T.Classify({0,0,.6f,0})==EPlantMassRegion::Weighing);
    TestTrue(TEXT("below gate belongs to pipe only"),T.Classify({0,0,.599f,0})==EPlantMassRegion::Pipe);
    TestTrue(TEXT("top plane excluded from WH"),T.Classify({0,0,1.8f,0})!=EPlantMassRegion::Weighing);
    TestTrue(TEXT("outside actual cone not counted as WH by old bounding cylinder"),T.Classify({.3f,0,.75f,0})==EPlantMassRegion::Outside);
    TestTrue(TEXT("receiver inlet port overlap has unique receiver ownership"),T.Classify({2.77f,0,2.682f,0})==EPlantMassRegion::Receiver);
    TestTrue(TEXT("upstream spigot still pipe"),T.Classify({2.74f,0,2.682f,0})==EPlantMassRegion::Pipe);
    TestTrue(TEXT("inlet protruding into receiver bore stays pipe until its actual end plane"),T.Classify({2.76f,0,2.682f,0})==EPlantMassRegion::Pipe);
    TestTrue(TEXT("receiver bore away from inlet is receiver inventory"),T.Classify({2.76f,0,2.8f,0})==EPlantMassRegion::Receiver);
    TestTrue(TEXT("below receiver gate is outside transfer"),T.Classify({3,0,2.15f,0})==EPlantMassRegion::Outside);
    // Existing network model: no independent carrier inlet. This inspects copied field data only.
    auto Network=F.C.plant_data;
    powder::plant_set_manual_valves(Network,F.Routes,1000,0,0,true,true);
    for(const auto& Route:F.Routes)if(Route.phase==1)
        TestEqual(TEXT("all supplier valves closed means zero carrier flow even with WH open"),Network[Route.cells.front()+2].x,0.f);
    powder::plant_set_manual_valves(Network,F.Routes,1000,4,4,true,true);
    for(const auto& Route:F.Routes)if(Route.phase==1)
        TestTrue(TEXT("empty C branch can supply carrier flow in the existing network model"),Network[Route.cells.front()+2].x>0);
    F.P[0].x=std::numeric_limits<float>::quiet_NaN();T.Observe(F.P,1,false,false);
    TestTrue(TEXT("invalid coordinate is data unavailable, never zero inventory"),Read(T).WeighingMass.Validity!=EPlantValueValidity::Valid);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlantTransferCohortTest,"ProcessUI.Transfer.CohortCompletion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlantTransferCohortTest::RunTest(const FString&)
{
    FTransferFixture F;FPlantTransferTelemetry T(F.C,F.Routes,{});T.Begin(F.P,10,7,false);
    auto S=Read(T);const double M=T.ParticleMassKg;
    TestTrue(TEXT("pre-open baseline includes both initial WH IDs"),FMath::Abs(S.Transfer.BaselineKg-2*M)<1e-12&&S.Transfer.OpeningControlRevision==7&&S.Transfer.StartedSimulationSeconds==10);
    TestEqual(TEXT("old receiver excluded from new arrivals"),S.Transfer.ArrivedKg,0.0);
    F.P[0]={0,0,.55f,0};T.Observe(F.P,10.1,true,false);S=Read(T);
    TestTrue(TEXT("first movement retained by pre-application capture"),S.Transfer.RemainingKg==M&&S.Transfer.InTransitKg==M);
    F.P[1]={3.05f,0,2.6f,0};T.Observe(F.P,10.2,true,false);S=Read(T);
    TestTrue(TEXT("empty WH is not complete while pipe contains material"),S.WeighingMass.Kg==0&&S.Transfer.State==EPlantTransferState::Moving&&S.Transfer.ArrivedKg==M);
    F.P[0]={3,0,2.6f,0};T.Observe(F.P,10.3,true,false);
    for(int I=0;I<30;++I)T.Observe(F.P,10.3,true,false);
    TestTrue(TEXT("paused repeated observations do not advance stability"),Read(T).Transfer.State==EPlantTransferState::Moving&&Read(T).Transfer.StableSeconds==0);
    T.Observe(F.P,10.6,true,false);TestTrue(TEXT("short stable interval not complete"),Read(T).Transfer.State==EPlantTransferState::Moving);
    T.Observe(F.P,10.9,true,false);S=Read(T);
    TestTrue(TEXT("normal transfer completes after mass conditions and simulation time"),S.Transfer.State==EPlantTransferState::Complete&&FMath::Abs(S.Transfer.ArrivedKg-2*M)<1e-12);
    TestTrue(TEXT("outlet open remains independent of completed transfer"),S.Transfer.OutletAppliedOpen);
    TestTrue(TEXT("existing receiver mass not counted as arrival"),FMath::Abs(S.ReceiverMass.Kg-3*M)<1e-12&&S.Transfer.InitialReceiverKg==M);
    F.P[0]={0,0,.55f,0};T.Observe(F.P,11,true,false);
    TestTrue(TEXT("backflow revokes current completion and resets stable interval"),Read(T).Transfer.State==EPlantTransferState::Moving&&Read(T).Transfer.ArrivedKg==M&&Read(T).Transfer.StableSeconds==0);
    F.P[0]={3,0,2.6f,0};T.Observe(F.P,11.1,true,false);T.Observe(F.P,11.7,false,false);S=Read(T);
    TestTrue(TEXT("recrossing never doubles received mass"),FMath::Abs(S.Transfer.ArrivedKg-2*M)<1e-12&&S.Transfer.State==EPlantTransferState::Complete&&!S.Transfer.OutletAppliedOpen);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlantTransferInvalidTest,"ProcessUI.Transfer.Invalidations",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlantTransferInvalidTest::RunTest(const FString&)
{
    FTransferFixture F;
    for(int Case=0;Case<5;++Case)
    {
        auto P=F.P;FPlantTransferTelemetry T(F.C,F.Routes,{});
        if(Case==0)P[3]={0,0,.55f,0};
        T.Begin(P,1,1,false);
        if(Case==1)P[0]={1.5f,1,1,0};
        if(Case==2)P[3]={0,0,1.2f,0};
        if(Case==4)P[2]={3,0,2.15f,0};
        T.Observe(P,2,true,Case==3);
        TestTrue(TEXT("old pipe / unclassified cohort / inflow / receiver discharge prevent completion"),Read(T).Transfer.State==EPlantTransferState::Indeterminate&&!Read(T).Transfer.Reason.IsEmpty());
        if(Case==2)TestTrue(TEXT("inflow explains the affected region"),Read(T).Transfer.Reason.Contains(TEXT("칭량호퍼에 새 원료")));
        if(Case==4)TestTrue(TEXT("old receiver departure has its own cause"),Read(T).Transfer.Reason.Contains(TEXT("기존 수취 원료")));
        T.Observe(F.P,20,false,false);TestTrue(TEXT("indeterminate cannot recover silently"),Read(T).Transfer.State==EPlantTransferState::Indeterminate);
    }
    FPlantTransferTelemetry Empty(F.C,F.Routes,{});auto P=F.P;P[0]=P[1]=P[3];Empty.Begin(P,0,1,false);Empty.Observe(P,100,true,false);
    TestTrue(TEXT("initially empty is no material, not completed"),Read(Empty).Transfer.State==EPlantTransferState::Empty);
    FPlantTransferTelemetry Reset(F.C,F.Routes,{});Reset.Begin(F.P,1,5,false);Reset.ResetConnection(2);Reset.Observe(F.P,1,true,false);
    TestTrue(TEXT("connection change invalidates cohort even with open gate"),Read(Reset).Transfer.Id==0&&Read(Reset).Transfer.Connection==2&&Read(Reset).Transfer.State==EPlantTransferState::Idle);
    FPlantTransferTelemetry NewEpoch(F.C,F.Routes,{});NewEpoch.Observe(F.P,0,false,false);
    TestTrue(TEXT("new worker starts without previous transfer"),Read(NewEpoch).Transfer.Id==0&&Read(NewEpoch).Transfer.ArrivedKg==0);
    FPlantTransferSettings Invalid;Invalid.StableSimulationSeconds=-1;FPlantTransferTelemetry Bad(F.C,F.Routes,Invalid);Bad.Begin(F.P,0,1,false);
    TestTrue(TEXT("invalid criteria does not disable control or report false completion"),Read(Bad).Transfer.State==EPlantTransferState::Indeterminate&&Read(Bad).WeighingMass.Validity==EPlantValueValidity::Valid);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlantMassFormattingTest,"ProcessUI.Transfer.Formatting",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlantMassFormattingTest::RunTest(const FString&)
{
    FPlantMassValue M;M.Validity=EPlantValueValidity::Valid;
    TestEqual(TEXT("valid empty is 0 g"),UProcessUIStateAdapter::FormatMass(M),FString(TEXT("0 g")));
    M.Kg=.000040212;TestEqual(TEXT("small mass in grams"),UProcessUIStateAdapter::FormatMass(M),FString(TEXT("0.040 g")));
    M.Validity=EPlantValueValidity::Unknown;TestEqual(TEXT("unknown is never zero"),UProcessUIStateAdapter::FormatMass(M),FString(TEXT("데이터 없음")));
    FPlantTransferSnapshot NoAir;NoAir.Validity=EPlantValueValidity::Valid;NoAir.State=EPlantTransferState::Moving;
    NoAir.OutletAppliedOpen=true;NoAir.CarrierFlowValid=true;
    TestTrue(TEXT("confirmed zero carrier flow explains delay without claiming blockage"),UProcessUIStateAdapter::FormatTransfer(NoAir).Contains(TEXT("이송 공기 유량 0")));
    NoAir.CarrierFlowValid=false;TestFalse(TEXT("unknown carrier flow does not claim a confirmed zero"),UProcessUIStateAdapter::FormatTransfer(NoAir).Contains(TEXT("이송 공기 유량 0")));
    FPlantSimulationSnapshot S;S.Validity=EPlantValueValidity::Valid;S.PublishedWallSeconds=10;S.WeighingMass.Validity=EPlantValueValidity::Valid;S.WeighingMass.Kg=.2;S.Transfer.Validity=EPlantValueValidity::Valid;S.Transfer.State=EPlantTransferState::Complete;
    auto Stale=UProcessUIStateAdapter::BuildEquipment(TEXT("weigher"),&S,20);
    TestTrue(TEXT("stale hides both mass and transfer completion"),Stale.CurrentMass.Validity==EPlantValueValidity::Stale&&UProcessUIStateAdapter::FormatTransfer(Stale.Transfer).Contains(TEXT("데이터 없음")));
    FText Status,Quantity,Valve;UProcessUIStateAdapter::Format(UProcessUIStateAdapter::BuildEquipment(TEXT("weigher"),nullptr,10),Status,Quantity,Valve);
    TestEqual(TEXT("disconnected quantity cleared"),Quantity.ToString(),FString(TEXT("데이터 없음")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlantLowPolyTransferTest,"ProcessUI.Transfer.LowPolyCohort",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlantLowPolyTransferTest::RunTest(const FString&)
{
    FTransferFixture F(TEXT("SourceAssets/ProcessPlant_20261111_LowPoly/Simulation"));
    FPlantTransferTelemetry T(F.C,F.Routes,{});
    T.Observe(F.P,0,false,false);
    TestEqual(TEXT("new dataset has six supplier gates"),int32(powder::plant_supplier_gates(F.C.plant_data).size()),6);
    TestTrue(TEXT("actual float radius and runtime density"),FMath::Abs(T.ParticleMassKg-4.0212391695907155e-5)<1e-14);
    TestTrue(TEXT("new geometry mass valid"),Read(T).WeighingMass.Validity==EPlantValueValidity::Valid);
    TestTrue(TEXT("WH gate center belongs only to WH"),T.Classify({0,0,.6f,0})==EPlantMassRegion::Weighing);
    TestTrue(TEXT("downstream gate plane belongs to pipe"),T.Classify({0,0,.599f,0})==EPlantMassRegion::Pipe);
    TestTrue(TEXT("spigot overlap still in pipe"),T.Classify({2.76f,0,2.682f,0})==EPlantMassRegion::Pipe);
    TestTrue(TEXT("receiver bore after inlet is arrival"),T.Classify({2.77f,0,2.682f,0})==EPlantMassRegion::Receiver);
    TestTrue(TEXT("receiver filter core/bore is not arrived powder"),T.Classify({3,0,3.1f,0})==EPlantMassRegion::Outside);
    TestTrue(TEXT("storage annulus outside filter is receiver"),T.Classify({3.15f,0,3.1f,0})==EPlantMassRegion::Receiver);
    TestTrue(TEXT("tube sheet and clean-air module excluded"),T.Classify({3.15f,0,3.39f,0})==EPlantMassRegion::Outside&&T.Classify({3.15f,0,3.5f,0})==EPlantMassRegion::Outside);
    T.Begin(F.P,1,10,false);const auto Start=Read(T).Transfer;const double M=T.ParticleMassKg;
    F.P[0]={3,0,2.6f,0};T.Observe(F.P,1.1,true,false);
    T.Begin(F.P,1.1,11,false);
    TestTrue(TEXT("repeated open preserves original cohort and first arrival"),Read(T).Transfer.Id==Start.Id&&Read(T).Transfer.BaselineKg==Start.BaselineKg&&Read(T).Transfer.ArrivedKg==M);
    T.Observe(F.P,1.2,false,false); // early close / full stop
    for(int I=0;I<10;++I)T.Observe(F.P,1.2,false,false);
    T.Begin(F.P,1.2,20,false);
    TestTrue(TEXT("resume keeps initial baseline and revision"),Read(T).Transfer.Id==Start.Id&&Read(T).Transfer.OpeningControlRevision==10&&Read(T).Transfer.ArrivedKg==M);
    F.P[1]={0,0,.55f,0};T.Observe(F.P,1.3,true,false);
    TestTrue(TEXT("pipe residue prevents completion"),Read(T).Transfer.InTransitKg==M&&Read(T).Transfer.State==EPlantTransferState::Moving);
    F.P[1]={3.05f,0,2.6f,0};T.Observe(F.P,1.4,true,false);T.Observe(F.P,1.4,false,false);
    TestEqual(TEXT("paused stability does not advance"),Read(T).Transfer.StableSeconds,0.0);
    T.Observe(F.P,2,false,false);
    TestTrue(TEXT("separate fixture reaches mass+simulation-time completion"),Read(T).Transfer.State==EPlantTransferState::Complete&&Read(T).Transfer.ArrivedKg==2*M&&Read(T).Transfer.InitialReceiverKg==M);
    F.P[0]={0,0,.55f,0};T.Observe(F.P,2.1,true,false);T.Begin(F.P,2.1,30,false);
    TestTrue(TEXT("backflow/reopen never double counts"),Read(T).Transfer.Id==Start.Id&&Read(T).Transfer.ArrivedKg==M&&Read(T).Transfer.State==EPlantTransferState::Moving);
    F.P[3]={0,0,1.2f,0};T.Observe(F.P,2.2,true,false);T.Begin(F.P,2.3,31,false);
    TestTrue(TEXT("inflow fault is not cleared by reopening"),Read(T).Transfer.State==EPlantTransferState::Indeterminate&&Read(T).Transfer.Id==Start.Id);
    T.ResetConnection(9);T.Observe(F.P,2.3,true,false);
    TestTrue(TEXT("actor/connection change discards old cohort"),Read(T).Transfer.Id==0&&Read(T).Transfer.BaselineKg==0);
    return true;
}
#endif
