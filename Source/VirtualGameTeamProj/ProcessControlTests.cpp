#include "ProcessControlTypes.h"
#include "ProcessControlService.h"
#include "PowderPlantTest.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "powder/plant.hpp"
#include <limits>

namespace
{
struct FControlFixture
{
    FProcessControlMachine M;
    double Now=100;
    int64 Revision=0;
    TArray<EProcessCommand> Sent;
    FProcessControlTransport Send;
    FControlFixture()
    {
        auto& S=M.Snapshot;S.Validity=EPlantValueValidity::Valid;S.Execution=EPlantExecutionState::Running;
        S.Epoch=1;S.Sequence=1;S.PublishedWallSeconds=Now;S.RequestedSuppliers={false,false,false};S.RequestedRunning=true;
        S.RequestedRPM=S.WorkerTargetRPM=15;S.AppliedRPMValid=true;
        M.Connect(S);M.TimeoutPolicy=EProcessTimeoutPolicy::Freeze;M.PausePolicy=EProcessPausePolicy::CloseThenPause;M.PausedClosePolicy=EProcessPausedClosePolicy::StopAll;
        Send=[this](EProcessCommand C,const FString&,double,int64,int64& Rev,FString&){Sent.Add(C);Rev=++Revision;return true;};
    }
    FProcessCommandRecord Request(const TCHAR* Id,EProcessCommand C,double V=0,FGuid Guid=FGuid())
    {return M.Submit(Guid.IsValid()?Guid:FGuid::NewGuid(),M.Snapshot.Epoch,Id,C,V,Now,Send);}
    void Publish(){auto S=M.Snapshot;S.Sequence++;S.PublishedWallSeconds=Now;S.ProcessedControlRevision=Revision;M.Observe(S,Now,Send);}
    void Manual(){M.ManualGranted=true;}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcessCommandProtocolTest,"ProcessUI.Control.Protocol",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProcessCommandProtocolTest::RunTest(const FString&)
{
    FControlFixture F;F.Manual();
    auto Open=F.Request(TEXT("supply-c"),EProcessCommand::Valve,1);
    TestTrue(TEXT("dispatch is not completion"),Open.Status==EProcessCommandStatus::Processing);
    auto Again=F.Request(TEXT("supply-c"),EProcessCommand::Valve,1,Open.RequestId);
    TestEqual(TEXT("same request transmitted once"),F.Sent.Num(),1);
    TestEqual(TEXT("same revision on retry"),Again.Revision,Open.Revision);
    F.Publish();TestTrue(TEXT("later snapshot alone cannot confirm different applied valve"),F.M.History[Open.RequestId].IsPending());
    F.M.Snapshot.RequestedSuppliers[2]=true;F.M.Snapshot.AppliedSupplierMask=4;F.Publish();
    TestTrue(TEXT("worker-confirmed open"),F.M.History[Open.RequestId].Status==EProcessCommandStatus::Applied);
    Again=F.Request(TEXT("supply-c"),EProcessCommand::Valve,1);
    TestTrue(TEXT("repeated desired open stays open"),Again.Status==EProcessCommandStatus::Applied);
    TestEqual(TEXT("already open sends no toggle"),F.Sent.Num(),1);
    TestTrue(TEXT("request id payload collision rejected"),F.Request(TEXT("supply-c"),EProcessCommand::Valve,0,Open.RequestId).Status==EProcessCommandStatus::Rejected);
    TestTrue(TEXT("D unsupported"),F.Request(TEXT("supply-d"),EProcessCommand::Valve,1).Status==EProcessCommandStatus::Rejected);
    for(double RPM:{-1.,31.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
        TestTrue(TEXT("invalid rpm rejected"),F.Request(TEXT("mixer"),EProcessCommand::MixerRPM,RPM).Status==EProcessCommandStatus::Rejected);
    auto Close=F.Request(TEXT("supply-c"),EProcessCommand::Valve,0);
    F.M.Snapshot.RequestedSuppliers[2]=false;F.Publish();
    TestTrue(TEXT("occupied gate remains pending"),F.M.History[Close.RequestId].IsPending());
    TestTrue(TEXT("reopening cannot overtake pending close"),F.Request(TEXT("supply-c"),EProcessCommand::Valve,1).Status==EProcessCommandStatus::Rejected);
    F.M.Snapshot.AppliedSupplierMask=0;F.Publish();
    TestTrue(TEXT("clear gate completes"),F.M.History[Close.RequestId].Status==EProcessCommandStatus::Applied);
    F.M.Snapshot.Execution=EPlantExecutionState::Paused;F.M.Snapshot.RequestedRunning=false;
    TestTrue(TEXT("paused opening rejected"),F.Request(TEXT("supply-a"),EProcessCommand::Valve,1).Status==EProcessCommandStatus::Rejected);
    auto RPM=F.Request(TEXT("mixer"),EProcessCommand::MixerRPM,7);
    F.M.Snapshot.RequestedRPM=F.M.Snapshot.WorkerTargetRPM=7;F.Publish();
    TestTrue(TEXT("paused setpoint acknowledged"),F.M.History[RPM.RequestId].Status==EProcessCommandStatus::Applied);
    TestFalse(TEXT("setpoint never starts mixer"),F.M.Snapshot.WorkerMixerEnabled);
    auto Resume=F.Request(TEXT("plant"),EProcessCommand::Resume,1);
    auto Reset=F.M.Snapshot;Reset.Epoch++;Reset.Sequence=1;F.M.Observe(Reset,F.Now,F.Send);
    TestTrue(TEXT("reset invalidates old request"),F.M.History[Resume.RequestId].Status==EProcessCommandStatus::Unknown);
    TestFalse(TEXT("reset revokes manual permission"),F.M.ManualGranted);
    auto Old=F.M.Submit(FGuid::NewGuid(),1,TEXT("plant"),EProcessCommand::Resume,1,F.Now,F.Send);
    TestTrue(TEXT("old epoch cannot execute"),Old.Status==EProcessCommandStatus::Rejected);
    F.M.Snapshot.Validity=EPlantValueValidity::Error;F.M.Snapshot.Execution=EPlantExecutionState::Fault;F.Publish();
    TestTrue(TEXT("fault cannot resume"),F.Request(TEXT("plant"),EProcessCommand::Resume,1).Status==EProcessCommandStatus::Rejected);
    FControlFixture Race;Race.Manual();
    Race.Request(TEXT("supply-a"),EProcessCommand::Valve,1);
    Race.M.Snapshot.RequestedControlRevision=1;Race.M.Snapshot.RequestedSuppliers[0]=true;
    Race.Request(TEXT("supply-a"),EProcessCommand::Valve,0);
    Race.M.Snapshot.RequestedControlRevision=2;Race.M.Snapshot.RequestedSuppliers[0]=false;
    // Old published closed state must not acknowledge a close behind an in-flight opening.
    TestTrue(TEXT("old closed snapshot is not already-closed proof"),Race.Request(TEXT("supply-a"),EProcessCommand::Valve,0).IsPending());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcessStopSequenceTest,"ProcessUI.Control.StopSequence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProcessStopSequenceTest::RunTest(const FString&)
{
    FControlFixture F;F.Manual();
    auto Open=F.Request(TEXT("supply-a"),EProcessCommand::Valve,1);
    auto Stop=F.Request(TEXT("plant"),EProcessCommand::StopAll);
    TestTrue(TEXT("stop overtakes open"),F.M.History[Open.RequestId].Status==EProcessCommandStatus::Unknown);
    F.M.Snapshot.AppliedSupplierMask=1;F.Publish();
    TestTrue(TEXT("occupied gate waits, not pause"),F.M.StopStage==EProcessStopStage::Closing);
    TestTrue(TEXT("no early pause command"),F.Sent.Last()==EProcessCommand::StopAll);
    TestTrue(TEXT("open rejected during stop"),F.Request(TEXT("supply-b"),EProcessCommand::Valve,1).Status==EProcessCommandStatus::Rejected);
    F.M.Snapshot.AppliedSupplierMask=0;F.Publish();
    TestTrue(TEXT("close then pause"),F.M.StopStage==EProcessStopStage::Pausing);
    TestFalse(TEXT("pause dispatch does not grant permission"),F.M.ManualGranted);
    F.M.Snapshot.Execution=EPlantExecutionState::Paused;F.M.Snapshot.RequestedRunning=false;F.Publish();
    TestTrue(TEXT("worker pause completes stop"),F.M.StopStage==EProcessStopStage::Complete && F.M.ManualGranted);
    TestTrue(TEXT("stop record applied"),F.M.History[Stop.RequestId].Status==EProcessCommandStatus::Applied);
    const int32 Count=F.Sent.Num();F.Request(TEXT("plant"),EProcessCommand::StopAll,0,Stop.RequestId);
    TestEqual(TEXT("replay of stop does not resume computation"),F.Sent.Num(),Count);
    FControlFixture Timed;Timed.M.StopTimeoutSeconds=.5;Timed.M.Snapshot.AppliedHopperOpen=true;
    Stop=Timed.Request(TEXT("plant"),EProcessCommand::StopAll);Timed.Now+=.6;Timed.Publish();
    TestTrue(TEXT("wall timeout fails despite frozen simulation time"),Timed.M.StopStage==EProcessStopStage::Failed);
    TestTrue(TEXT("explicit freeze policy requested pause"),Timed.Sent.Last()==EProcessCommand::Pause);
    TestFalse(TEXT("failed stop does not grant manual"),Timed.M.ManualGranted);
    TestTrue(TEXT("failure names weighing outlet"),Timed.M.History[Stop.RequestId].Message.Contains(TEXT("칭량호퍼")));
    FControlFixture Stale;Stale.Manual();Open=Stale.Request(TEXT("supply-a"),EProcessCommand::Valve,1);
    Stale.Now+=3;Stale.M.Observe(Stale.M.Snapshot,Stale.Now,Stale.Send);
    TestTrue(TEXT("stale pending request unknown"),Stale.M.History[Open.RequestId].Status==EProcessCommandStatus::Unknown);
    TestTrue(TEXT("stale prevents new commands including fake safety success"),Stale.Request(TEXT("plant"),EProcessCommand::StopAll).Status==EProcessCommandStatus::Rejected);
    FControlFixture Keep;Keep.M.TimeoutPolicy=EProcessTimeoutPolicy::KeepClosing;Keep.M.StopTimeoutSeconds=.5;Keep.M.Snapshot.AppliedReceiverOpen=true;
    Keep.Request(TEXT("plant"),EProcessCommand::StopAll);Keep.Now+=.6;Keep.Publish();
    TestTrue(TEXT("keep-closing timeout stays failed"),Keep.M.StopStage==EProcessStopStage::Failed);
    TestEqual(TEXT("keep-closing does not send freeze"),Keep.Sent.Num(),1);
    Keep.M.Snapshot.AppliedReceiverOpen=false;Keep.Publish();TestFalse(TEXT("late close never automatically grants manual"),Keep.M.ManualGranted);
    FControlFixture Paused;Paused.Manual();Paused.M.Snapshot.Execution=EPlantExecutionState::Paused;Paused.M.Snapshot.RequestedRunning=false;Paused.M.Snapshot.AppliedHopperOpen=Paused.M.Snapshot.RequestedHopperOpen=true;
    auto Safety=Paused.Request(TEXT("weigher"),EProcessCommand::Valve,0);
    TestTrue(TEXT("configured paused close uses coherent full-stop targets"),Safety.UsesStopSequence&&Paused.Sent.Last()==EProcessCommand::StopAll);
    FControlFixture NoPolicy;NoPolicy.M.TimeoutPolicy=EProcessTimeoutPolicy::Unspecified;
    TestTrue(TEXT("unspecified policy does not silently choose freeze or continue"),NoPolicy.Request(TEXT("plant"),EProcessCommand::StopAll).Status==EProcessCommandStatus::Rejected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcessFreezeAckTest,"ProcessUI.Control.FreezeAcknowledgement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProcessFreezeAckTest::RunTest(const FString&)
{
    FControlFixture F;F.M.StopTimeoutSeconds=.5;F.M.CommandTimeoutSeconds=.4;F.M.Snapshot.AppliedHopperOpen=true;
    auto Stop=F.Request(TEXT("plant"),EProcessCommand::StopAll);F.Now+=.6;F.Publish();
    TestTrue(TEXT("timeout sends freeze but has no worker acknowledgement"),F.M.FreezeState==EProcessFreezeState::Awaiting);
    TestTrue(TEXT("failed stop still names blocked WH101"),F.M.StopFailureEquipment.Contains(TEXT("칭량호퍼")));
    auto Old=F.M.Snapshot;Old.Execution=EPlantExecutionState::Paused;Old.RequestedRunning=false;
    F.M.Observe(Old,F.Now,F.Send);
    TestTrue(TEXT("old sequence/revision cannot confirm freeze"),F.M.FreezeState==EProcessFreezeState::Awaiting);
    F.Publish();
    TestTrue(TEXT("new revision and worker paused confirms freeze"),F.M.FreezeState==EProcessFreezeState::Confirmed);
    TestTrue(TEXT("freeze is not successful closure"),F.M.StopStage==EProcessStopStage::Failed && F.M.History[Stop.RequestId].Status==EProcessCommandStatus::Failed);
    TestFalse(TEXT("freeze grants no manual permission"),F.M.ManualGranted);
    TestTrue(TEXT("resume blocked after failed closure"),F.Request(TEXT("plant"),EProcessCommand::Resume,1).Status==EProcessCommandStatus::Rejected);
    int32 Count=F.Sent.Num();F.Request(TEXT("plant"),EProcessCommand::StopAll,0,Stop.RequestId);
    TestEqual(TEXT("same stop id never retries"),F.Sent.Num(),Count);
    auto Retry=F.Request(TEXT("plant"),EProcessCommand::StopAll);
    TestTrue(TEXT("explicit new retry resumes closure processing"),Retry.UsesStopSequence && Retry.IsPending() && F.Sent.Last()==EProcessCommand::StopAll);
    TestTrue(TEXT("retry clears prior freeze tracking"),F.M.FreezeState==EProcessFreezeState::None);
    F.M.Observe(Old,F.Now,F.Send);
    TestFalse(TEXT("old pause response cannot complete retry"),F.M.ManualGranted);
    F.M.Snapshot.Execution=EPlantExecutionState::Running;F.M.Snapshot.RequestedRunning=true;F.Publish();
    TestTrue(TEXT("blocked valve still waits on retry"),F.M.StopStage==EProcessStopStage::Closing);
    F.M.Snapshot.AppliedHopperOpen=false;F.Publish();
    TestTrue(TEXT("cleared retry requests final pause"),F.M.StopStage==EProcessStopStage::Pausing);
    F.M.Snapshot.Execution=EPlantExecutionState::Paused;F.M.Snapshot.RequestedRunning=false;F.Publish();
    TestTrue(TEXT("retry grants manual only after closure and pause ack"),F.M.StopStage==EProcessStopStage::Complete && F.M.ManualGranted);
    TestTrue(TEXT("original failure remains failure"),F.M.History[Stop.RequestId].Status==EProcessCommandStatus::Failed);

    FControlFixture Missing;Missing.M.StopTimeoutSeconds=.5;Missing.M.CommandTimeoutSeconds=.4;Missing.M.Snapshot.AppliedReceiverOpen=true;
    Missing.Request(TEXT("plant"),EProcessCommand::StopAll);Missing.Now+=.6;Missing.Publish();
    Missing.Now+=.5;Missing.Publish();
    TestTrue(TEXT("freeze response wait is bounded in wall time"),Missing.M.FreezeState==EProcessFreezeState::Unconfirmed);
    Missing.M.Snapshot.Execution=EPlantExecutionState::Paused;Missing.M.Snapshot.RequestedRunning=false;Missing.Publish();
    TestTrue(TEXT("late pause does not silently recover failed transaction"),Missing.M.FreezeState==EProcessFreezeState::Unconfirmed && !Missing.M.ManualGranted);
    for(int32 Failure=0;Failure<3;++Failure)
    {
        FControlFixture Lost;Lost.M.StopTimeoutSeconds=.5;Lost.M.Snapshot.AppliedHopperOpen=true;
        Lost.Request(TEXT("plant"),EProcessCommand::StopAll);Lost.Now+=.6;Lost.Publish();
        if(Failure==0){Lost.Now+=3;Lost.M.Observe(Lost.M.Snapshot,Lost.Now,Lost.Send);}
        if(Failure==1){Lost.M.Snapshot.Validity=EPlantValueValidity::Error;Lost.M.Snapshot.Execution=EPlantExecutionState::Fault;Lost.Publish();}
        if(Failure==2)Lost.M.Disconnect(TEXT("test source destroyed"));
        TestTrue(TEXT("stale/error/disconnection cannot claim frozen"),Lost.M.FreezeState==EProcessFreezeState::Unconfirmed && !Lost.M.ManualGranted);
        TestTrue(TEXT("unhealthy worker cannot resume"),Lost.Request(TEXT("plant"),EProcessCommand::Resume,1).Status==EProcessCommandStatus::Rejected);
    }
    FControlFixture Reset;Reset.M.StopTimeoutSeconds=.5;Reset.M.Snapshot.AppliedHopperOpen=true;
    auto Previous=Reset.Request(TEXT("plant"),EProcessCommand::StopAll);Reset.Now+=.6;Reset.Publish();
    auto New=Reset.M.Snapshot;New.Epoch++;New.Sequence=1;Reset.M.Observe(New,Reset.Now,Reset.Send);
    TestTrue(TEXT("new epoch drops old freeze wait"),Reset.M.FreezeState==EProcessFreezeState::None && !Reset.M.ManualGranted);
    TestTrue(TEXT("old id cannot act on new epoch"),Reset.M.Submit(Previous.RequestId,Previous.Epoch,TEXT("plant"),EProcessCommand::StopAll,0,Reset.Now,Reset.Send).Status==EProcessCommandStatus::Rejected);
    FControlFixture Rejected;Rejected.M.StopTimeoutSeconds=.5;Rejected.M.Snapshot.AppliedHopperOpen=true;
    Rejected.Request(TEXT("plant"),EProcessCommand::StopAll);
    Rejected.Send=[](EProcessCommand,const FString&,double,int64,int64&,FString& Why){Why=TEXT("worker unavailable");return false;};
    Rejected.Now+=.6;Rejected.Publish();
    TestTrue(TEXT("failed freeze transport is never frozen"),Rejected.M.FreezeState==EProcessFreezeState::Unconfirmed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcessManualPausePolicyTest,"ProcessUI.Control.ManualPausePolicies",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProcessManualPausePolicyTest::RunTest(const FString&)
{
    FControlFixture F;F.Manual();
    F.M.Snapshot.Execution=EPlantExecutionState::Paused;F.M.Snapshot.RequestedRunning=false;
    auto Pending=F.Request(TEXT("plant"),EProcessCommand::Resume,1);
    // Resume is still unacknowledged: manual pause must preempt it rather than wait.
    F.M.Snapshot.Execution=EPlantExecutionState::Running;F.M.Snapshot.RequestedRunning=true;
    F.M.Snapshot.AppliedSupplierMask=4;F.M.Snapshot.WorkerMixerEnabled=F.M.Snapshot.RequestedMixerEnabled=true;F.M.Snapshot.AppliedRPM=15;
    auto Pause=F.Request(TEXT("plant"),EProcessCommand::Pause);
    TestTrue(TEXT("manual pause dispatches full stop with priority"),Pause.UsesStopSequence && F.Sent.Last()==EProcessCommand::StopAll && F.M.History[Pending.RequestId].Status==EProcessCommandStatus::Unknown);
    F.Publish();TestTrue(TEXT("manual pause waits for occupied gate and rpm zero"),F.M.StopStage==EProcessStopStage::Closing);
    TestTrue(TEXT("mixer start blocked while pausing"),F.Request(TEXT("mixer"),EProcessCommand::MixerEnabled,1).Status==EProcessCommandStatus::Rejected);
    F.M.Snapshot.AppliedSupplierMask=0;F.M.Snapshot.RequestedMixerEnabled=F.M.Snapshot.WorkerMixerEnabled=false;F.M.Snapshot.AppliedRPM=0;F.Publish();
    TestTrue(TEXT("manual pause sends pause only after closure"),F.M.StopStage==EProcessStopStage::Pausing);
    F.M.Snapshot.Execution=EPlantExecutionState::Paused;F.M.Snapshot.RequestedRunning=false;F.Publish();
    TestTrue(TEXT("manual pause completes on worker ack"),F.M.History[Pause.RequestId].Status==EProcessCommandStatus::Applied && F.M.ManualGranted);

    // Selected C is already closed, but WH101 is open in this paused state.
    F.M.Snapshot.AppliedHopperOpen=F.M.Snapshot.RequestedHopperOpen=true;
    auto Close=F.Request(TEXT("supply-c"),EProcessCommand::Valve,0);
    TestTrue(TEXT("even already-closed selected valve routes paused close through full stop"),Close.UsesStopSequence && Close.IsPending() && F.Sent.Last()==EProcessCommand::StopAll);
    TestTrue(TEXT("paused closure announces temporary computation"),Close.Message.Contains(TEXT("계산")) && Close.Message.Contains(TEXT("재개")));
    TestTrue(TEXT("opening blocked during paused-close sequence"),F.Request(TEXT("supply-a"),EProcessCommand::Valve,1).Status==EProcessCommandStatus::Rejected);
    F.M.Snapshot.Execution=EPlantExecutionState::Running;F.M.Snapshot.RequestedRunning=true;F.Publish();
    TestTrue(TEXT("another valve blocks completion"),F.M.StopStage==EProcessStopStage::Closing);
    F.M.Snapshot.RequestedHopperOpen=F.M.Snapshot.AppliedHopperOpen=false;F.Publish();
    F.M.Snapshot.Execution=EPlantExecutionState::Paused;F.M.Snapshot.RequestedRunning=false;F.Publish();
    TestTrue(TEXT("paused close returns to acknowledged pause"),F.M.History[Close.RequestId].Status==EProcessCommandStatus::Applied && F.M.ManualGranted);
    const int32 Count=F.Sent.Num();auto Closed=F.Request(TEXT("supply-c"),EProcessCommand::Valve,0);
    TestTrue(TEXT("whole plant already closed and paused uses confirmed shortcut"),Closed.UsesStopSequence && Closed.Status==EProcessCommandStatus::Applied);
    TestEqual(TEXT("confirmed shortcut does not advance simulation"),F.Sent.Num(),Count);
    for(EProcessCommand C:{EProcessCommand::Pause,EProcessCommand::Valve})
    {
        FControlFixture Timeout;Timeout.Manual();Timeout.M.StopTimeoutSeconds=.5;Timeout.M.Snapshot.AppliedHopperOpen=true;
        if(C==EProcessCommand::Valve){Timeout.M.Snapshot.Execution=EPlantExecutionState::Paused;Timeout.M.Snapshot.RequestedRunning=false;}
        auto R=Timeout.Request(C==EProcessCommand::Pause?TEXT("plant"):TEXT("supply-c"),C);
        Timeout.Now+=.6;Timeout.Publish();
        TestTrue(TEXT("both pause and paused-close timeouts use freeze protocol"),R.UsesStopSequence && Timeout.M.StopStage==EProcessStopStage::Failed && Timeout.M.FreezeState==EProcessFreezeState::Awaiting);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcessClosureGuardTest,"ProcessUI.Control.PhysicalClosureGuard",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProcessClosureGuardTest::RunTest(const FString&)
{
    // Real unchanged collision-clearance function, controlled fixture (not a live GPU gate claim).
    powder::Grain Gate{0,0,1,.1f};const float Radius=.00125f;
    TestFalse(TEXT("particle on gate prevents closure"),powder::plant_supplier_closure_clear(Gate,{{0,0,1,0}},Radius));
    TestTrue(TEXT("particle released below gate allows closure"),powder::plant_supplier_closure_clear(Gate,{{0,0,.9f,0}},Radius));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcessLiveControlTest,"ProcessUI.Control.LiveWorker",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProcessLiveControlTest::RunTest(const FString&)
{
    UWorld::InitializationValues IV;IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());
    auto* A=W->SpawnActor<APowderPlantTest>();A->Running=false;A->DispatchBeginPlay();
    auto* Service=NewObject<UProcessControlService>(W);Service->Attach(A);
    TestTrue(TEXT("ordinary service defaults activate approved manual policies"),Service->State.TimeoutPolicy==EProcessTimeoutPolicy::Freeze && Service->State.PausePolicy==EProcessPausePolicy::CloseThenPause && Service->State.PausedClosePolicy==EProcessPausedClosePolicy::StopAll);
    auto Wait=[&](TFunctionRef<bool()> Done){double End=FPlatformTime::Seconds()+45;while(FPlatformTime::Seconds()<End){Service->Tick();if(Done())return true;if(Service->State.Snapshot.Validity==EPlantValueValidity::Error){AddError(Service->State.Snapshot.Error);return false;}FPlatformProcess::Sleep(.02f);}AddError(TEXT("live worker control timeout"));return false;};
    bool OK=Wait([&](){return Service->State.Snapshot.Validity==EPlantValueValidity::Valid;});
    auto Request=[&](const TCHAR* Id,EProcessCommand C,double V){auto R=Service->Request(Id,C,V);if(R.Status==EProcessCommandStatus::Rejected){AddError(R.Message);return false;}return Wait([&](){return Service->State.History[R.RequestId].Status==EProcessCommandStatus::Applied;});};
    if(OK)OK=Request(TEXT("plant"),EProcessCommand::StopAll,0);
    if(OK){const double T=A->GetSimulationSnapshot().PhysicalSeconds;FPlatformProcess::Sleep(.3f);Service->Tick();TestEqual(TEXT("stop completion freezes physical time"),A->GetSimulationSnapshot().PhysicalSeconds,T);
        OK=Request(TEXT("mixer"),EProcessCommand::MixerRPM,7);TestFalse(TEXT("paused RPM does not start mixer"),A->GetSimulationSnapshot().RequestedMixerEnabled);
        OK=Request(TEXT("plant"),EProcessCommand::Resume,1)&&OK;OK=Wait([&](){return A->GetSimulationSnapshot().PhysicalSeconds>T;})&&OK;
        bool KeptManual=true;
        const double ObserveUntil=FPlatformTime::Seconds()+3;
        while(OK&&FPlatformTime::Seconds()<ObserveUntil)
        {
            Service->Tick();KeptManual&=Service->State.ManualGranted;
            FPlatformProcess::Sleep(.002f);
        }
        TestTrue(TEXT("repeated live snapshot polling preserves confirmed manual access"),KeptManual);
    }
    if(OK){auto R=Service->Request(TEXT("supply-c"),EProcessCommand::Valve,1);const auto Again=Service->Request(TEXT("supply-c"),EProcessCommand::Valve,1,R.RequestId);TestEqual(TEXT("live duplicate revision"),Again.Revision,R.Revision);OK=Wait([&](){return Service->State.History[R.RequestId].Status==EProcessCommandStatus::Applied;});
        OK=Request(TEXT("supply-c"),EProcessCommand::Valve,1)&&OK;TestTrue(TEXT("duplicate open stays applied open"),(A->GetSimulationSnapshot().AppliedSupplierMask&4)!=0);
        OK=Request(TEXT("supply-c"),EProcessCommand::Valve,0)&&OK;
        for(const TCHAR* Id:{TEXT("weigher"),TEXT("receiver")}){
            OK=Request(Id,EProcessCommand::Valve,1)&&OK;
            if(FString(Id)==TEXT("weigher")){
                const auto Applied=A->GetSimulationSnapshot();
                TestTrue(TEXT("actual uploaded field reports zero carrier flow with all suppliers closed"),Applied.Transfer.CarrierFlowValid&&Applied.Transfer.CalculatedCarrierFlowM3s==0);
            }
            OK=Request(Id,EProcessCommand::Valve,0)&&OK;
        }
        OK=Request(TEXT("mixer"),EProcessCommand::MixerEnabled,1)&&OK;TestEqual(TEXT("GPU committed rpm"),A->GetSimulationSnapshot().AppliedRPM,7.0);
        OK=Request(TEXT("mixer"),EProcessCommand::MixerEnabled,0)&&OK;
        TestTrue(TEXT("live invalid RPM rejected"),Service->Request(TEXT("mixer"),EProcessCommand::MixerRPM,31).Status==EProcessCommandStatus::Rejected);
        TestTrue(TEXT("live unimplemented D rejected"),Service->Request(TEXT("supply-d"),EProcessCommand::Valve,1).Status==EProcessCommandStatus::Rejected);
        OK=Request(TEXT("supply-c"),EProcessCommand::Valve,1)&&OK;
        OK=Request(TEXT("mixer"),EProcessCommand::MixerEnabled,1)&&OK;
        OK=Request(TEXT("plant"),EProcessCommand::Pause,0)&&OK;
        const auto Paused=A->GetSimulationSnapshot();
        TestTrue(TEXT("normal manual pause closes valves and stops mixer before worker pause"),Paused.Execution==EPlantExecutionState::Paused && !Paused.RequestedRunning && Paused.AppliedSupplierMask==0 && !Paused.AppliedHopperOpen && !Paused.AppliedReceiverOpen && Paused.AppliedRPMValid && Paused.AppliedRPM==0);
        const auto Closed=Service->Request(TEXT("supply-c"),EProcessCommand::Valve,0);
        TestTrue(TEXT("normal paused close uses full-stop verification"),Closed.UsesStopSequence && Closed.Status==EProcessCommandStatus::Applied);
        FPlatformProcess::Sleep(.3f);Service->Tick();TestEqual(TEXT("manual pause and already-closed request preserve stopped physical time"),A->GetSimulationSnapshot().PhysicalSeconds,Paused.PhysicalSeconds);
        auto Pending=Service->Request(TEXT("plant"),EProcessCommand::Resume,1);const int64 Epoch=Pending.Epoch;A->ResetSimulation();Service->Tick();
        TestTrue(TEXT("real reset fences old request"),Service->State.History[Pending.RequestId].Status==EProcessCommandStatus::Unknown);
        TestTrue(TEXT("old epoch rejected after actual reset"),Service->RequestForEpoch(FGuid::NewGuid(),Epoch,TEXT("plant"),EProcessCommand::Resume,1).Status==EProcessCommandStatus::Rejected);
    }
    Service->Detach();A->Destroy();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return OK;
}
#endif
