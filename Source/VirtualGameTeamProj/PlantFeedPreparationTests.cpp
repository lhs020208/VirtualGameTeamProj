#include "PlantFeedPreparation.h"
#include "ProcessUIStateAdapter.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include <limits>
#if WITH_DEV_AUTOMATION_TESTS
namespace
{
struct FReadyFixture
{
    powder::GranularConfig C;
    std::vector<powder::PlantRoute> Routes;
    std::vector<powder::Grain> P{{0,0,1.1f,0},{.05f,0,1.1f,0},{3,0,2.5f,0},{-1.2f,1.4f,2.7f,0}};
    FPlantControlTargets Command;
    FReadyFixture()
    {
        const auto Root=FPaths::ProjectDir()/TEXT("SourceAssets/ProcessPlant_20261111_LowPoly/Simulation");
        C.plant=true;C.radius=.002f;C.count=unsigned(P.size());
        C.plant_data=powder::read_plant_data(TCHAR_TO_UTF8(*(Root/TEXT("runtime.plant"))));
        Routes=powder::read_plant_routes(TCHAR_TO_UTF8(*(Root/TEXT("runtime.routes"))));
        Command.Revision=Command.WeighingOpenRevision=10;Command.ObservationConnection=1;Command.WeighingOpen=true;
        Command.WeighingOpenDeadline=2000;Command.WeighingOpenLeaseUntil=2000;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlantFeedReadyGeometry,"ProcessUI.FeedPreparation.GeometryAndAtomicBaseline",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlantFeedReadyGeometry::RunTest(const FString&)
{
    FReadyFixture F;FPlantTransferTelemetry T(F.C,F.Routes,{});FPlantFeedPreparation G(F.C,F.Routes,T,.5,true);
    for(int I=0;I<6;++I)
    {
        const auto Gate=F.C.plant_data[powder::plant_supplier_gates(F.C.plant_data)[I]];
        TestEqual(TEXT("each actual supply gate downstream bore has a readiness region"),G.SupplyPath({Gate.x,Gate.y,Gate.z-.02f,0}),I);
        TestEqual(TEXT("material above closed gate is excluded"),G.SupplyPath({Gate.x,Gate.y,Gate.z+.02f,0}),INDEX_NONE);
    }
    TestEqual(TEXT("A inlet rectangular cell includes off-axis lower pocket"),G.SupplyPath({-.25f,-.30f,1.66f,0}),0);
    TestEqual(TEXT("C inlet rectangular cell includes off-axis lower pocket"),G.SupplyPath({.25f,-.30f,1.66f,0}),2);
    TestEqual(TEXT("WH inventory is not a supply residue"),G.SupplyPath({0,0,1.79f,0}),INDEX_NONE);
    TestEqual(TEXT("RH inventory is not a supply residue"),G.SupplyPath({3,0,2.5f,0}),INDEX_NONE);
    F.Command.Suppliers=1;
    TestFalse(TEXT("requested open supplier blocks admission"),G.Evaluate(F.P,1,1,F.Command,0,false));
    TestTrue(TEXT("explicit valve wait state"),G.Snapshot().State==EPlantFeedPreparationState::WaitingValves);
    F.Command.Suppliers=0;
    TestFalse(TEXT("actual open supplier blocks admission despite requested close"),G.Evaluate(F.P,2,2,F.Command,1,false));
    F.P[3]={-.25f,-.30f,1.71f,0};
    TestFalse(TEXT("actual connector residual blocks admission"),G.Evaluate(F.P,3,3,F.Command,0,false));
    TestTrue(TEXT("residual count and reason"),G.Snapshot().State==EPlantFeedPreparationState::WaitingSupplyResidual&&G.Snapshot().SupplyResidualCount==1);
    F.P[3]={-.15f,0,1.7f,0};
    TestFalse(TEXT("clear supply starts stability, does not open immediately"),G.Evaluate(F.P,4,4,F.Command,0,false));
    for(int I=0;I<10;++I)G.Evaluate(F.P,4,5+I,F.Command,0,false);
    TestEqual(TEXT("repeated frozen simulation time cannot accumulate stability"),G.Snapshot().StableSeconds,0.0);
    TestFalse(TEXT("short stable interval waits"),G.Evaluate(F.P,4.49,20,F.Command,0,false));
    // Same count/mass but a different particle ID must invalidate stability.
    F.P[0]={-1.2f,1.4f,2.7f,0};F.P[2]={0,0,1.1f,0};
    TestFalse(TEXT("equal mass is not equal composition"),G.Evaluate(F.P,4.5,21,F.Command,0,false));
    TestEqual(TEXT("ID change resets timer"),G.Snapshot().StableSeconds,0.0);
    TestTrue(TEXT("exact stable current particle state is ready"),G.Evaluate(F.P,5,22,F.Command,0,false));
    const auto Revision=F.Command.WeighingOpenRevision;
    T.Begin(F.P,5,Revision,false);G.Opened(5,5);
    FPlantSimulationSnapshot S;T.Publish(S);
    TestEqual(TEXT("baseline is taken from the admission snapshot, including late pre-open material"),S.Transfer.BaselineKg,3*T.ParticleMassKg);
    TestEqual(TEXT("baseline/open worker token is preserved"),S.Transfer.OpeningControlRevision,Revision);
    TestTrue(TEXT("supply confirmation precedes baseline and actual open at same solver time"),G.Snapshot().SupplyConfirmedSimulationSeconds==5&&G.Snapshot().BaselineCapturedSimulationSeconds==5&&G.Snapshot().OutletAppliedSimulationSeconds==5);
    TestTrue(TEXT("completed preparation is labelled as opening-time evidence, not current supply inventory"),UProcessUIStateAdapter::FormatPreparation(G.Snapshot()).Contains(TEXT("개방 시")));
    F.P[0]={0,0,1.2f,0};T.Observe(F.P,5.1,true,false);T.Observe(F.P,5.2,true,false);T.Publish(S);
    TestTrue(TEXT("real post-open inflow still invalidates without changing baseline"),S.Transfer.State==EPlantTransferState::Indeterminate&&S.Transfer.AdditionalInflowCount==1&&S.Transfer.BaselineKg==3*T.ParticleMassKg);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlantFeedReadyCancellation,"ProcessUI.FeedPreparation.CancellationAndFailure",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlantFeedReadyCancellation::RunTest(const FString&)
{
    FReadyFixture F;FPlantTransferTelemetry T(F.C,F.Routes,{});
    for(int Case=0;Case<6;++Case)
    {
        FPlantFeedPreparation G(F.C,F.Routes,T,.5,true);auto C=F.Command;
        G.Evaluate(F.P,1,1,C,0,false);
        if(Case==0){C.WeighingOpen=false;C.WeighingOpenRevision=0;} // stop/close
        if(Case==1){C.ObservationConnection++;C.WeighingOpen=false;C.WeighingOpenRevision=0;} // detach/rebind
        if(Case==2)C.WeighingOpenDeadline=1.1;
        if(Case==3)C.WeighingOpenLeaseUntil=1.1;
        if(Case==4)C.Running=false;
        if(Case==5){C=FPlantControlTargets();} // reset creates a fresh worker/mailbox
        TestFalse(TEXT("cancel, disconnect, expiry, stale or pause cannot admit"),G.Evaluate(F.P,2,2,C,0,false));
        C.Running=true;C.WeighingOpenLeaseUntil=2000;C.WeighingOpenDeadline=2000;
        TestFalse(TEXT("resume or renewed lease never resurrects an expired/cancelled request"),G.Evaluate(F.P,100,3,C,0,false));
        C.WeighingOpen=true;C.WeighingOpenRevision=C.Revision=11;
        TestFalse(TEXT("explicit new revision starts a new stable interval"),G.Evaluate(F.P,100,4,C,0,false));
        TestTrue(TEXT("fresh retry can pass original criteria"),G.Evaluate(F.P,100.5,5,C,0,false));
    }
    FPlantFeedPreparation Changed(F.C,F.Routes,T,.5,true);auto C=F.Command;
    Changed.Evaluate(F.P,1,1,C,0,false);C.Suppliers=1;++C.Revision;
    Changed.SyncIntent(C,1.2,2);C.Suppliers=0;++C.Revision;
    TestFalse(TEXT("supply open/close pulse invalidates previously accumulated readiness"),Changed.Evaluate(F.P,2,3,C,0,false));
    TestEqual(TEXT("readiness restarts after command state change"),Changed.Snapshot().StableSeconds,0.0);
    FPlantFeedPreparation Empty(F.C,F.Routes,T,.5,true);auto P=F.P;P[0]=P[1]=P[3];
    TestFalse(TEXT("empty hopper does not open"),Empty.Evaluate(P,1,1,F.Command,0,false));
    TestTrue(TEXT("empty has explicit retry state"),Empty.Snapshot().State==EPlantFeedPreparationState::Empty);
    FPlantFeedPreparation Bad(F.C,F.Routes,T,.5,true);P[0].x=std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("invalid particle data fail closed"),Bad.Evaluate(P,1,1,F.Command,0,false));
    FPlantFeedPreparation FlowTest(F.C,F.Routes,T,.5,false);
    TestFalse(TEXT("guard is disabled outside the designated level"),FlowTest.Snapshot().Enabled);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlantFeedReadyProtocol,"ProcessUI.FeedPreparation.CommandProtocol",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlantFeedReadyProtocol::RunTest(const FString&)
{
    FProcessControlMachine M;FPlantSimulationSnapshot S;double Now=100;int64 Sent=0;
    S.Epoch=1;S.Sequence=1;S.PublishedWallSeconds=Now;S.Validity=EPlantValueValidity::Valid;S.Execution=EPlantExecutionState::Running;
    S.RequestedRunning=true;S.RequestedSuppliers.Init(false,6);S.AppliedRPMValid=true;S.FeedPreparation.Enabled=true;
    M.Connect(S);M.ManualGranted=true;M.TimeoutPolicy=EProcessTimeoutPolicy::Freeze;M.PausePolicy=EProcessPausePolicy::CloseThenPause;
    FProcessControlTransport Send=[&](EProcessCommand,const FString&,double,int64,int64& R,FString&){R=++Sent;return true;};
    auto R=M.Submit(FGuid::NewGuid(),1,TEXT("weigher"),EProcessCommand::Valve,1,Now,Send);
    TestTrue(TEXT("opening uses a preparation deadline instead of 10-second ordinary response limit"),R.PreparedOpening&&R.Deadline==Now+1800&&M.DispatchDeadline==R.Deadline);
    M.Submit(R.RequestId,1,TEXT("weigher"),EProcessCommand::Valve,1,Now,Send);
    TestEqual(TEXT("duplicate opening sends exactly one token"),Sent,int64(1));
    S.RequestedHopperOpen=true;S.ProcessedControlRevision=S.RequestedControlRevision=R.Revision;S.FeedPreparation.RequestRevision=R.Revision;
    S.FeedPreparation.State=EPlantFeedPreparationState::WaitingSupplyResidual;S.FeedPreparation.Reason=TEXT("공급 잔류 대기");
    Now+=11;S.Sequence++;S.PublishedWallSeconds=Now;M.Observe(S,Now,Send);
    TestTrue(TEXT("waiting is processing and exposes worker reason"),M.History[R.RequestId].IsPending()&&M.History[R.RequestId].Message==S.FeedPreparation.Reason);
    S.AppliedHopperOpen=true;S.FeedPreparation.RequestRevision=999;S.FeedPreparation.State=EPlantFeedPreparationState::Opened;S.Sequence++;M.Observe(S,Now,Send);
    TestTrue(TEXT("another opening token cannot acknowledge this RequestId"),M.History[R.RequestId].IsPending());
    S.FeedPreparation.RequestRevision=R.Revision;S.Sequence++;M.Observe(S,Now,Send);
    TestTrue(TEXT("same token, actual open and committed response confirm application"),M.History[R.RequestId].Status==EProcessCommandStatus::Applied);
    S.AppliedHopperOpen=S.RequestedHopperOpen=false;S.Sequence++;M.Observe(S,Now,Send);
    auto Pending=M.Submit(FGuid::NewGuid(),1,TEXT("weigher"),EProcessCommand::Valve,1,Now,Send);
    auto Stop=M.Submit(FGuid::NewGuid(),1,TEXT("plant"),EProcessCommand::StopAll,0,Now,Send);
    TestFalse(TEXT("StopAll cancels pending opening record"),M.History[Pending.RequestId].IsPending());
    TestTrue(TEXT("same cancelled RequestId cannot resend later"),M.Submit(Pending.RequestId,1,TEXT("weigher"),EProcessCommand::Valve,1,Now,Send).Status==EProcessCommandStatus::Unknown);
    S.Epoch++;S.Sequence=1;M.Observe(S,Now,Send);
    TestTrue(TEXT("old epoch request is rejected"),M.Submit(FGuid::NewGuid(),1,TEXT("weigher"),EProcessCommand::Valve,1,Now,Send).Status==EProcessCommandStatus::Rejected);
    S.Validity=EPlantValueValidity::Valid;S.PublishedWallSeconds=Now;S.FeedPreparation.State=EPlantFeedPreparationState::Ready;
    auto Old=UProcessUIStateAdapter::BuildEquipment(TEXT("weigher"),&S,Now+3);
    TestFalse(TEXT("stale UI never displays old readiness"),Old.FeedPreparation.Enabled);
    return true;
}
#endif
