#include "ProcessControlTypes.h"
#include "ProcessUIStateAdapter.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/Paths.h"
#include "powder/plant_pressure.hpp"
#include <cstring>
#include <limits>

namespace {
struct FPressureFixture
{
    FProcessControlMachine M;
    double Now=100;int64 Revision=0;TArray<EProcessCommand> Sent;FProcessControlTransport Send;
    FPressureFixture()
    {
        FPlantSimulationSnapshot S;S.Validity=EPlantValueValidity::Valid;S.Execution=EPlantExecutionState::Running;
        S.Epoch=1;S.Sequence=1;S.PublishedWallSeconds=Now;S.RequestedRunning=true;S.RequestedSuppliers.Init(false,6);
        S.AppliedRPMValid=true;S.PressureControlEnabled=true;S.AppliedPressureValid=true;
        M.Connect(S);M.ManualGranted=true;M.TimeoutPolicy=EProcessTimeoutPolicy::Freeze;
        M.PausePolicy=EProcessPausePolicy::CloseThenPause;M.PausedClosePolicy=EProcessPausedClosePolicy::StopAll;
        Send=[this](EProcessCommand C,const FString&,double,int64,int64& R,FString&){Sent.Add(C);R=++Revision;return true;};
    }
    FProcessCommandRecord Request(const TCHAR* Id,EProcessCommand C,double V=0,FGuid Guid=FGuid())
    {return M.Submit(Guid.IsValid()?Guid:FGuid::NewGuid(),M.Snapshot.Epoch,Id,C,V,Now,Send);}
    void Publish(){auto S=M.Snapshot;++S.Sequence;S.PublishedWallSeconds=Now;S.ProcessedControlRevision=Revision;M.Observe(S,Now,Send);}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPressureProtocolTest,"ProcessUI.Pressure.Protocol",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPressureProtocolTest::RunTest(const FString&)
{
    FPressureFixture F;
    const double Pa=.125*PlantControl::PaPerKPa;
    TestEqual(TEXT("kPa to Pa"),Pa,125.0);TestEqual(TEXT("absolute atmosphere"),PlantControl::AtmosphericPa+Pa,101450.0);
    for(const TCHAR* Id:{TEXT("supply-a"),TEXT("supply-b"),TEXT("supply-c"),TEXT("supply-d"),TEXT("supply-e"),TEXT("supply-f")})
        TestEqual(TEXT("every supported supply shares reservoir 0"),PlantControl::PressureTarget(Id),0);
    TestEqual(TEXT("WH index"),PlantControl::PressureTarget(TEXT("weigher")),1);
    TestEqual(TEXT("RH index"),PlantControl::PressureTarget(TEXT("receiver")),2);
    for(double V:{-1001.,1001.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
        TestTrue(TEXT("invalid pressure rejected"),F.Request(TEXT("weigher"),EProcessCommand::Pressure,V).Status==EProcessCommandStatus::Rejected);
    TestTrue(TEXT("unknown target rejected"),F.Request(TEXT("mixer"),EProcessCommand::Pressure,1).Status==EProcessCommandStatus::Rejected);
    TestTrue(TEXT("RH positive rejected"),F.Request(TEXT("receiver"),EProcessCommand::Pressure,1).Status==EProcessCommandStatus::Rejected);
    F.M.Snapshot.AppliedReceiverOpen=true;
    TestTrue(TEXT("vented RH cannot acknowledge suction"),F.Request(TEXT("receiver"),EProcessCommand::Pressure,-500).Status==EProcessCommandStatus::Rejected);
    F.M.Snapshot.AppliedReceiverOpen=false;
    auto R=F.Request(TEXT("supply-a"),EProcessCommand::Pressure,Pa);
    TestTrue(TEXT("send requires worker confirmation"),R.IsPending());
    F.Request(TEXT("supply-a"),EProcessCommand::Pressure,Pa,R.RequestId);
    TestEqual(TEXT("duplicate sends once"),F.Sent.Num(),1);
    TestTrue(TEXT("other supply cannot overtake common pending request"),F.Request(TEXT("supply-f"),EProcessCommand::Pressure,500).Status==EProcessCommandStatus::Rejected);
    F.M.Snapshot.RequestedGaugePressurePa.X=Pa;F.M.Snapshot.AppliedGaugePressurePa.X=Pa;F.Publish();
    TestTrue(TEXT("value without field upload revision insufficient"),F.M.History[R.RequestId].IsPending());
    F.M.Snapshot.PressureAppliedRevision=R.Revision;F.Publish();
    TestTrue(TEXT("confirmed field upload completes"),F.M.History[R.RequestId].Status==EProcessCommandStatus::Applied);
    const auto E=UProcessUIStateAdapter::BuildEquipment(TEXT("supply-f"),&F.M.Snapshot,F.Now);
    TestEqual(TEXT("F observes common applied pressure"),E.AppliedPressurePa.Value,Pa);
    TestTrue(TEXT("boundary meaning"),E.AppliedPressurePa.Unit.Contains(TEXT("boundary")));
    TestTrue(TEXT("stale pressure invalidated"),UProcessUIStateAdapter::BuildEquipment(TEXT("supply-f"),&F.M.Snapshot,F.Now+3).AppliedPressurePa.Validity==EPlantValueValidity::Stale);
    FPlantTransferSnapshot T;T.Validity=EPlantValueValidity::Valid;T.State=EPlantTransferState::Moving;T.CarrierRegulated=true;T.CarrierFlowValid=true;T.OutletAppliedOpen=true;
    const FString NoFlow=UProcessUIStateAdapter::FormatTransfer(T);
    TestTrue(TEXT("regulated zero flow explains pressure boundary"),NoFlow.Contains(TEXT("적용 압력 설정")));
    TestFalse(TEXT("regulated model does not instruct opening F"),NoFlow.Contains(TEXT("독립 이송 공기 공급은 미지원")));
    T.CalculatedCarrierFlowM3s=-.01;
    TestTrue(TEXT("reverse flow labelled"),UProcessUIStateAdapter::FormatTransfer(T).Contains(TEXT("역방향")));
    F.M.Snapshot.Execution=EPlantExecutionState::Paused;F.M.Snapshot.RequestedRunning=false;
    TestTrue(TEXT("no paused pressure queue"),F.Request(TEXT("weigher"),EProcessCommand::Pressure,500).Status==EProcessCommandStatus::Rejected);
    F.M.Snapshot.Execution=EPlantExecutionState::Running;F.M.Snapshot.RequestedRunning=true;
    R=F.Request(TEXT("weigher"),EProcessCommand::Pressure,500);
    auto Reset=F.M.Snapshot;++Reset.Epoch;Reset.Sequence=1;Reset.RequestedGaugePressurePa=Reset.AppliedGaugePressurePa=FVector::ZeroVector;F.M.Observe(Reset,F.Now,F.Send);
    TestTrue(TEXT("reset invalidates pending pressure"),F.M.History[R.RequestId].Status==EProcessCommandStatus::Unknown);
    TestTrue(TEXT("old epoch rejected"),F.M.Submit(FGuid::NewGuid(),1,TEXT("weigher"),EProcessCommand::Pressure,500,F.Now,F.Send).Status==EProcessCommandStatus::Rejected);
    TestTrue(TEXT("old RequestId rejected after reset"),F.Request(TEXT("weigher"),EProcessCommand::Pressure,500,R.RequestId).Status==EProcessCommandStatus::Rejected);
    F.M.Snapshot.PressureControlEnabled=false;F.M.ManualGranted=true;
    TestTrue(TEXT("legacy FlowTest has no new pressure commands"),F.Request(TEXT("supply-a"),EProcessCommand::Pressure,100).Status==EProcessCommandStatus::Rejected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPressureStopTest,"ProcessUI.Pressure.StopSequence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPressureStopTest::RunTest(const FString&)
{
    FPressureFixture F;auto Pending=F.Request(TEXT("weigher"),EProcessCommand::Pressure,500);
    F.M.Snapshot.RequestedGaugePressurePa=F.M.Snapshot.AppliedGaugePressurePa=FVector(125,500,-1000);
    const auto Stop=F.Request(TEXT("plant"),EProcessCommand::StopAll);
    TestTrue(TEXT("stop invalidates pressure request"),F.M.History[Pending.RequestId].Status==EProcessCommandStatus::Unknown);
    F.M.Snapshot.RequestedGaugePressurePa=FVector::ZeroVector;F.Publish();
    TestTrue(TEXT("closed valves alone cannot pause"),F.Sent.Last()==EProcessCommand::StopAll);
    TestTrue(TEXT("unconfirmed common pressure named"),F.M.UnconfirmedEquipment().Contains(TEXT("공급 공통")));
    TestTrue(TEXT("unconfirmed WH pressure named"),F.M.UnconfirmedEquipment().Contains(TEXT("칭량호퍼 압력")));
    F.M.Snapshot.AppliedGaugePressurePa=FVector::ZeroVector;F.M.Snapshot.AppliedPressureValid=false;F.Publish();
    TestTrue(TEXT("invalid zero settings cannot pause"),F.Sent.Last()==EProcessCommand::StopAll);
    F.M.Snapshot.AppliedPressureValid=true;F.Publish();
    TestTrue(TEXT("all zero/closed/RPM acknowledged before pause"),F.Sent.Last()==EProcessCommand::Pause);
    TestFalse(TEXT("send not pause acknowledgement"),F.M.ManualGranted);
    F.M.Snapshot.Execution=EPlantExecutionState::Paused;F.M.Snapshot.RequestedRunning=false;F.Publish();
    TestTrue(TEXT("stop confirmed"),F.M.StopStage==EProcessStopStage::Complete&&F.M.History[Stop.RequestId].Status==EProcessCommandStatus::Applied);
    F.Request(TEXT("plant"),EProcessCommand::Resume,1);F.M.Snapshot.Execution=EPlantExecutionState::Running;F.M.Snapshot.RequestedRunning=true;F.Publish();
    TestEqual(TEXT("resume generates no old pressure command"),F.Sent.Num(),4);
    TestTrue(TEXT("old pressure request remains terminal"),!F.M.History[Pending.RequestId].IsPending());
    FPressureFixture Timed;Timed.M.StopTimeoutSeconds=.5;Timed.M.Snapshot.AppliedGaugePressurePa.Y=500;
    Timed.Request(TEXT("plant"),EProcessCommand::StopAll);Timed.Now+=.6;Timed.Publish();
    TestTrue(TEXT("pressure timeout freezes"),Timed.M.StopStage==EProcessStopStage::Failed&&Timed.Sent.Last()==EProcessCommand::Pause);
    TestTrue(TEXT("timeout names WH setting"),Timed.M.StopFailureEquipment.Contains(TEXT("칭량호퍼 압력")));
    Timed.M.Snapshot.Execution=EPlantExecutionState::Paused;Timed.M.Snapshot.RequestedRunning=false;Timed.Publish();
    TestTrue(TEXT("freeze worker acknowledgement"),Timed.M.FreezeState==EProcessFreezeState::Confirmed);
    TestFalse(TEXT("freeze cannot grant manual permission"),Timed.M.ManualGranted);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPressureModelTest,"ProcessUI.Pressure.SourceModelGPU",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPressureModelTest::RunTest(const FString&)
{
    try {
        const FString Root=FPaths::ProjectDir()/TEXT("SourceAssets/ProcessPlant_20261111_LowPoly/Simulation");
        auto D=powder::read_plant_data(TCHAR_TO_UTF8(*(Root/TEXT("runtime.plant")))),Original=D;
        auto Routes=powder::read_plant_routes(TCHAR_TO_UTF8(*(Root/TEXT("runtime.routes"))));
        for(auto P:{powder::PlantPressureTargets{NAN,0,0},{0,INFINITY,0},{1001,0,0},{0,-1001,0},{0,0,1},{0,0,-1001}})
        {bool Rejected=false;try{powder::plant_set_regulated_pressures(D,Routes,P,3,3,true,true,false);}catch(const std::invalid_argument&){Rejected=true;}
            TestTrue(TEXT("invalid field update atomic rejection"),Rejected&&std::memcmp(D.data(),Original.data(),D.size()*sizeof(D[0]))==0);}
        for(int Stage=0;Stage<6;++Stage)for(unsigned Mask:{0u,1u,2u,3u,63u})for(bool WH:{false,true})for(bool RH:{false,true})
        {
            auto P=powder::plant_pressure_preset(Stage);auto State=powder::plant_set_regulated_pressures(D,Routes,P,Mask,Mask,WH,WH,RH);
            TestEqual(TEXT("RH vent override"),State.applied.receiver,RH?0.:P.receiver);unsigned Branch=0;double In=0,Out=0;
            for(const auto& R:Routes){double Q=D[R.cells[0]+2].x,DP=R.phase==0?P.supply-P.weighing:P.weighing-State.applied.receiver;
                bool Enabled=R.phase==0?(Mask&(1u<<Branch++))!=0:WH;
                TestTrue(TEXT("closed and signed flow"),(!Enabled||DP==0)?Q==0:Q*DP>0);
                for(int At:R.cells)TestEqual(TEXT("route continuity"),double(D[At+2].x),Q);
                if(R.phase==0)In+=Q;else Out=Q;
            }
            TestTrue(TEXT("WH ideal regulator balance"),std::abs(In+State.weighing_regulator_m3s-Out)<1e-7);
        }
        auto Independent=powder::plant_set_regulated_pressures(D,Routes,{0,500,-1000},0,0,true,true,false);
        TestTrue(TEXT("WH independent flow with F and all suppliers closed"),Independent.transfer_m3s>0&&Independent.weighing_regulator_m3s==Independent.transfer_m3s);
        powder::plant_set_regulated_pressures(D,Routes,{500,-500,-1000},3,0,true,false,false);
        for(const auto& R:Routes)TestEqual(TEXT("pending closure cuts carrier before gate closure"),D[R.cells.front()+2].x,0.f);
        for(auto P:{powder::PlantPressureTargets{1000,-1000,-1000},{-1000,1000,0}}){powder::plant_set_regulated_pressures(D,Routes,P,63,63,true,true,false);powder::validate_plant_data(D);}
        // Source 9cc5c177 one-particle GPU regression, separate from unchanged production 20,000-particle PIE.
        powder::GranularConfig C;C.plant=true;C.plant_threaded=true;C.kernel_optimize=true;C.plant_wall_mode=5;
        C.radius=.002f;C.dt=.00001f;C.iterations=8;C.count=1;C.plant_data=D;
        std::vector<powder::Grain> Seed{{2.3f,0,2.682f,0}},Zero(1);powder::GranularGPU GPU(C,Seed,Zero);
        double VX[3]{};for(int I=0;I<3;++I){powder::plant_set_regulated_pressures(D,Routes,{0,I==0?500.:I==1?-500.:0.,0},0,0,true,true,false);
            GPU.update_plant_field(D);GPU.set_state(Seed,Zero);GPU.advance(16,0,false);VX[I]=GPU.velocity_snapshot()[0].x;}
        TestTrue(TEXT("actual GPU signed pressure drag"),VX[0]>0&&VX[1]<0&&std::abs(VX[2])<1e-7);
        for(int I=0;I<30;++I){powder::plant_set_regulated_pressures(D,Routes,powder::plant_pressure_preset(I%6),3,3,I%2!=0,I%2!=0,I%3==0);
            GPU.update_plant_field(D);GPU.set_state(Seed,Zero);GPU.advance(16,0,false);auto P=GPU.snapshot()[0];TestTrue(TEXT("repeated live upload finite"),std::isfinite(P.x+P.y+P.z));}
        AddInfo(FString::Printf(TEXT("source model: 120 route/valve configurations; independent transfer q=%.12g m3/s; actual GPU vx +500/-500/0 Pa = %.12g / %.12g / %.12g"),Independent.transfer_m3s,VX[0],VX[1],VX[2]));
    } catch(const std::exception& E){AddError(UTF8_TO_TCHAR(E.what()));return false;}
    return true;
}
#endif
