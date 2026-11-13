#include "ProcessControlService.h"
#include "PowderPlantTest.h"
#include "ProcessPlantBinding.h"
#include "HAL/PlatformTime.h"

bool UProcessControlService::Attach(APowderPlantTest* Actor)
{
    if(Source.Get()==Actor && State.Connected)return true;
    Detach();
    if(!IsValid(Actor)||!Actor->ClaimControl(this))return false;
    Source=Actor;State.CommandTimeoutSeconds=CommandTimeoutSeconds;State.StopTimeoutSeconds=StopTimeoutSeconds;
    State.TransferPreparationTimeoutSeconds=Actor->TransferPreparationTimeoutSeconds;
    State.TimeoutPolicy=StopFailurePolicy==TEXT("Freeze")?EProcessTimeoutPolicy::Freeze:StopFailurePolicy==TEXT("KeepClosing")?EProcessTimeoutPolicy::KeepClosing:EProcessTimeoutPolicy::Unspecified;
    State.PausePolicy=ManualPausePolicy==TEXT("PreserveRequests")?EProcessPausePolicy::PreserveRequests:ManualPausePolicy==TEXT("CloseThenPause")?EProcessPausePolicy::CloseThenPause:EProcessPausePolicy::Unspecified;
    State.PausedClosePolicy=PausedClosePolicy==TEXT("StopAll")?EProcessPausedClosePolicy::StopAll:PausedClosePolicy==TEXT("RequireStopButton")?EProcessPausedClosePolicy::RequireStopButton:EProcessPausedClosePolicy::Unspecified;
    State.Connect(Actor->GetSimulationSnapshot());return true;
}

void UProcessControlService::Detach()
{
    State.Disconnect(TEXT("연결이 해제되었습니다. 이전 명령 결과는 미확인입니다."));
    if(auto* Actor=Source.Get())Actor->ReleaseControl(this);
    Source.Reset();
}

FProcessControlTransport UProcessControlService::Transport()
{
    return [this](EProcessCommand C,const FString& Id,double V,int64 Epoch,int64& Revision,FString& Error)
    {
        auto* A=Source.Get();
        if(!A || A->ControlOwner.Get()!=this){Error=TEXT("제어 대상 연결이 끊겼습니다.");return false;}
        switch(C){
        case EProcessCommand::Valve:
            if(Id==TEXT("weigher"))return A->SetWeighingValve(V!=0,Epoch,Revision,Error,State.DispatchDeadline);
            if(Id==TEXT("receiver"))return A->SetReceiverValve(V!=0,Epoch,Revision,Error);
            if(ProcessPlantBinding::SupplierIndex(Id)!=INDEX_NONE)return A->SetSupplierValve(ProcessPlantBinding::SupplierIndex(Id),V!=0,Epoch,Revision,Error);
            break;
        case EProcessCommand::MixerEnabled:return A->SetMixerEnabled(V!=0,Epoch,Revision,Error);
        case EProcessCommand::MixerRPM:return A->SetMixerRPM(V,Epoch,Revision,Error);
        case EProcessCommand::Pressure:return A->SetGaugePressure(PlantControl::PressureTarget(Id),V,Epoch,Revision,Error);
        case EProcessCommand::PurgePressure:return A->SetSupplyAPurge(true,V,Epoch,Revision,Error);
        case EProcessCommand::PurgeEnabled:return A->SetSupplyAPurge(false,V,Epoch,Revision,Error);
        case EProcessCommand::StopAll:return A->RequestAllClosed(Epoch,Revision,Error);
        case EProcessCommand::Resume:return A->SetSimulationRunning(true,Epoch,Revision,Error);
        case EProcessCommand::Pause:return A->SetSimulationRunning(false,Epoch,Revision,Error);
        }
        Error=TEXT("지원하지 않는 명령입니다.");return false;
    };
}

void UProcessControlService::Tick()
{
    check(IsInGameThread());
    if(auto* A=Source.Get())
    {
        // Function argument evaluation order is unspecified. Read time after the worker snapshot
        // so a publication during the copy cannot appear to come from the future and revoke manual access.
        const auto Snapshot=A->GetSimulationSnapshot();
        const double ObservedAt=FPlatformTime::Seconds();
        State.Observe(Snapshot,ObservedAt,Transport());
        // A waiting opening needs a fresh service observation, not just a queued bool in the mailbox.
        // Timeout/stale/reset/disconnect records cannot leave an opening armed for a later resume.
        for(const auto& Pair:State.History)
        {
            const auto& R=Pair.Value;
            if((R.Command==EProcessCommand::PurgePressure||R.Command==EProcessCommand::PurgeEnabled)&&R.Epoch==Snapshot.Epoch&&R.Connection==State.Connection&&R.Revision>0){
                const bool Fresh=Snapshot.Validity==EPlantValueValidity::Valid&&ObservedAt>=Snapshot.PublishedWallSeconds&&ObservedAt-Snapshot.PublishedWallSeconds<=2.;
                A->MaintainSupplyAPurge(R.Revision,Fresh&&State.ManualGranted&&!State.IsStopping()&&(R.IsPending()||R.Status==EProcessCommandStatus::Applied));
            }
            if(R.PreparedOpening&&R.Status!=EProcessCommandStatus::Applied&&R.Epoch==Snapshot.Epoch&&R.Connection==State.Connection&&R.Revision>0)
                A->MaintainPreparedOpening(R.Revision,R.IsPending());
        }
    }
    else if(State.Connected)State.Disconnect(TEXT("장비 연결이 끊겼습니다. 적용 결과를 확인할 수 없습니다."));
}

FProcessCommandRecord UProcessControlService::Request(const FString& Equipment,EProcessCommand Command,double Value,FGuid Id)
{
    Tick();
    return RequestForEpoch(Id.IsValid()?Id:FGuid::NewGuid(),State.Snapshot.Epoch,Equipment,Command,Value);
}

FProcessCommandRecord UProcessControlService::RequestForEpoch(FGuid Id,int64 Epoch,const FString& Equipment,EProcessCommand Command,double Value)
{
    Tick();
    auto Result=State.Submit(Id,Epoch,Equipment,Command,Value,FPlatformTime::Seconds(),Transport());
    UE_LOG(LogTemp,Display,TEXT("PROCESS_CONTROL request=%s epoch=%lld connection=%lld equipment=%s command=%d value=%.9g status=%s revision=%lld message=%s"),
        *Id.ToString(),Result.Epoch,Result.Connection,*Equipment,int32(Command),Value,*FProcessControlMachine::StatusText(Result.Status),Result.Revision,*Result.Message);
    return Result;
}
