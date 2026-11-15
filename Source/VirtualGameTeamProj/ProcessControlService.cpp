#include "ProcessControlService.h"
#include "PlantDoseValidation.h"
#include "PlantDoseTrace.h"
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
    State.Connect(Actor->GetSimulationSnapshot());AutoRun.Settings=AutoSettings;return true;
}

void UProcessControlService::Detach()
{
    CancelAutomatic(TEXT("연결 해제로 자동 실행을 취소했습니다. 정지 적용 응답은 미확인입니다."));
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
            if(Id==TEXT("weigher"))return A->SetWeighingValve(V!=0,Epoch,Revision,Error,State.DispatchDeadline,State.AutomationDispatch?AutoRun.State.BatchRevision:0);
            if(Id==TEXT("receiver"))return A->SetReceiverValve(V!=0,Epoch,Revision,Error,State.AutomationDispatch?AutoRun.State.BatchRevision:0,State.DispatchDeadline);
            if(ProcessPlantBinding::SupplierIndex(Id)!=INDEX_NONE){
                const bool Dose=State.AutomationDispatch&&V!=0&&AutoRun.State.BatchMode!=EProcessBatchMode::Full;
                return A->SetSupplierValve(ProcessPlantBinding::SupplierIndex(Id),V!=0,Epoch,Revision,Error,
                  Dose?AutoRun.State.TargetMassKg:-1,Dose?AutoRun.State.WeighingToleranceKg:0,Dose?AutoRun.State.BatchRevision:0,
                  Dose?State.DispatchRequestId:FGuid(),Dose?FPlatformTime::Seconds()+AutoRun.Settings.StageWallSeconds:0);
            }
            break;
        case EProcessCommand::MixerEnabled:return A->SetMixerEnabled(V!=0,Epoch,Revision,Error,State.AutomationDispatch?AutoRun.State.BatchRevision:0,State.DispatchDeadline);
        case EProcessCommand::MixerRPM:return A->SetMixerRPM(V,Epoch,Revision,Error);
        case EProcessCommand::Pressure:return A->SetGaugePressure(PlantControl::PressureTarget(Id),V,Epoch,Revision,Error);
        case EProcessCommand::FinalizeADose:return A->FinalizeADose(AutoRun.State.TargetMassKg,AutoRun.State.WeighingToleranceKg,AutoRun.State.BatchRevision,Epoch,Revision,Error,State.DispatchDeadline);
        case EProcessCommand::BeginAdditive:return A->BeginAdditive(AutoRun.State.AdditiveScheduled,AutoRun.State.AdditiveDelaySeconds,AutoRun.State.BatchRevision,Epoch,Revision,Error,State.DispatchDeadline);
        case EProcessCommand::BeginB:case EProcessCommand::CombineAB:return A->TransitionBatch(C==EProcessCommand::CombineAB,AutoRun.State.BatchRevision,Epoch,Revision,Error,State.DispatchDeadline);
        case EProcessCommand::RecoverBatch:return A->RecoverBatch(AutoRun.State.BatchRevision,Epoch,State.DispatchRequestId,Revision,Error,State.DispatchDeadline);
        case EProcessCommand::CaptureABatch:return A->CaptureInitialABatch(Epoch,Revision,Error);
        case EProcessCommand::PurgePressure:return A->SetSupplyAPurge(true,V,Epoch,Revision,Error,ProcessPlantBinding::SupplierIndex(Id));
        case EProcessCommand::PurgeEnabled:return A->SetSupplyAPurge(false,V,Epoch,Revision,Error,ProcessPlantBinding::SupplierIndex(Id));
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
    else if(State.Connected){CancelAutomatic(TEXT("해석 Actor 연결 해제로 자동 실행을 취소했습니다."));State.Disconnect(TEXT("장비 연결이 끊겼습니다. 적용 결과를 확인할 수 없습니다."));}
    const bool WasActive=AutoRun.State.Active;
    AutoRun.Tick(State,FPlatformTime::Seconds(),[this](EProcessCommand C,const FString& Id,double V){
        TGuardValue<bool> Dispatch(State.AutomationDispatch,true);
        const FGuid Request=FGuid::NewGuid();
        if(C==EProcessCommand::Valve&&Id==TEXT("supply-b")&&V==0)PlantDoseTrace::Write(TEXT("close_send"),1,State.Snapshot.PhysicalSeconds,State.Snapshot.Sequence,State.Snapshot.RequestedControlRevision,State.Snapshot.SupplyABatch.CaptureRevision,(State.Snapshot.SupplyABatch.InitialCount-State.Snapshot.SupplyABatch.InventorySourceCount)*State.Snapshot.ModelParticleMassKg,State.Snapshot.SupplyBBrake.PredictedSpillKg,AutoRun.State.CutoffEstimatedKg,State.Snapshot.PublishedWallSeconds,Request.ToString());
        const auto R=State.Submit(Request,AutoRun.State.Epoch,Id,C,V,FPlatformTime::Seconds(),Transport());
        UE_LOG(LogTemp,Display,TEXT("AUTO_A_COMMAND run=%s request=%s epoch=%lld command=%d equipment=%s value=%.9g revision=%lld time=%.9f status=%s reason=%s"),
          *AutoRun.State.RunId.ToString(),*R.RequestId.ToString(),R.Epoch,int32(C),*Id,V,R.Revision,State.Snapshot.PhysicalSeconds,*FProcessControlMachine::StatusText(R.Status),*R.Message);
        return R;
    });
    if(WasActive&&!AutoRun.State.Active&&AutoRun.State.Stage==EProcessAutoStage::Cancelled)
        if(auto* A=Source.Get())A->CancelAutomaticControls();
}

FProcessCommandRecord UProcessControlService::Request(const FString& Equipment,EProcessCommand Command,double Value,FGuid Id)
{
    if(Command==EProcessCommand::StopAll||Command==EProcessCommand::Pause)CancelAutomatic(TEXT("전체 정지 요청으로 자동 실행과 대기 명령을 취소했습니다. 계산 재개만으로 자동 복원하지 않습니다. 배치 상태 확인 후 별도로 재개하세요."));
    Tick();
    return RequestForEpoch(Id.IsValid()?Id:FGuid::NewGuid(),State.Snapshot.Epoch,Equipment,Command,Value);
}

FProcessCommandRecord UProcessControlService::RequestForEpoch(FGuid Id,int64 Epoch,const FString& Equipment,EProcessCommand Command,double Value)
{
    if(Command==EProcessCommand::StopAll||Command==EProcessCommand::Pause)CancelAutomatic(TEXT("전체 정지 요청으로 자동 실행과 대기 명령을 취소했습니다."));
    Tick();
    auto Result=State.Submit(Id,Epoch,Equipment,Command,Value,FPlatformTime::Seconds(),Transport());
    if(Command==EProcessCommand::MixerRPM&&!AutoRun.State.Active&&(Result.IsPending()||Result.Status==EProcessCommandStatus::Applied)){
        // Individual RPM controls remain authoritative for the next batch too.
        AutoSettings.TargetMixerRPM=Value;AutoRun.Settings=AutoSettings;
    }
    UE_LOG(LogTemp,Display,TEXT("PROCESS_CONTROL request=%s epoch=%lld connection=%lld equipment=%s command=%d value=%.9g status=%s revision=%lld message=%s"),
        *Id.ToString(),Result.Epoch,Result.Connection,*Equipment,int32(Command),Value,*FProcessControlMachine::StatusText(Result.Status),Result.Revision,*Result.Message);
    return Result;
}

bool UProcessControlService::SetAutomaticMixingDuration(double Seconds,FString& Reason)
{
 if(AutoRun.State.Active){Reason=TEXT("자동 배치 중 혼합 시간은 변경할 수 없습니다.");return false;}
 AutoRun.Settings=AutoSettings;
 if(!AutoRun.SetMixingDuration(Seconds,Reason))return false;
 AutoSettings=AutoRun.Settings;return true;
}
bool UProcessControlService::StartAutomaticA()
{
 Tick();AutoRun.Settings=AutoSettings;const bool WasActive=AutoRun.State.Active;
 const bool Started=AutoRun.Start(FGuid::NewGuid(),State,FPlatformTime::Seconds());
 if(Started&&!WasActive){const auto R=GetAppliedRecipe();AutoRun.State.RecipeName=R.Name;
  AutoRun.State.RecipeMaterialA=R.MaterialA;AutoRun.State.RecipeMaterialB=R.MaterialB;AutoRun.State.RecipeMaterialC=R.MaterialC;
  AutoRun.State.LockedRecipeJson=ProcessRecipe::Encode(R);
 }
 return Started;
}
void UProcessControlService::CancelAutomatic(const FString& Reason)
{
 if(!AutoRun.State.Active)return;
 AutoRun.State.StopRequestedSeconds=State.Snapshot.PhysicalSeconds;AutoRun.Cancel(Reason);State.AutomationActive=false;
 if(auto* A=Source.Get())A->CancelAutomaticControls();
}

bool UProcessControlService::SetAutomaticBatchMode(EProcessBatchMode Mode,FString& Reason)
{
 if(AutoRun.State.Active||State.IsStopping()){Reason=TEXT("자동 실행·전체 정지 중 배치 설정 변경 불가");return false;}
 if(Mode!=EProcessBatchMode::Full&&Mode!=EProcessBatchMode::TargetMass&&Mode!=EProcessBatchMode::SequentialAB){Reason=TEXT("지원하지 않는 계량 모드");return false;}
 AutoSettings.BatchMode=Mode;AutoRun.Settings=AutoSettings;Reason.Empty();return true;
}
bool UProcessControlService::SetAutomaticTargetMass(double TargetKg,double ToleranceKg,FString& Reason)
{
 if(AutoRun.State.Active||State.IsStopping()){Reason=TEXT("자동 실행·전체 정지 중 계량 설정 변경 불가");return false;}
 const auto& S=State.Snapshot;
 if(!State.Connected||!S.SupplyABatch.Valid||S.Validity!=EPlantValueValidity::Valid){Reason=TEXT("실제 A 공급 가능량을 확인할 수 없습니다.");return false;}
 Reason=PlantDose::Validate(TargetKg,ToleranceKg,S.ModelParticleMassKg,S.SupplyABatch.InventorySourceCount*S.ModelParticleMassKg);
 if(!Reason.IsEmpty())return false;
 AutoSettings.TargetMassKg=TargetKg;AutoSettings.WeighingToleranceKg=ToleranceKg;AutoRun.Settings=AutoSettings;return true;
}

bool UProcessControlService::SetAutomaticBTargetMass(double TargetKg,double ToleranceKg,FString& Reason)
{
 if(AutoRun.State.Active||State.IsStopping()){Reason=TEXT("자동 실행·전체 정지 중 B 설정 변경 불가");return false;}
 const auto& S=State.Snapshot;
 if(!State.Connected||!S.SupplyABatch.Valid||S.Validity!=EPlantValueValidity::Valid){Reason=TEXT("실제 B 공급 가능량 미확인");return false;}
 Reason=PlantDose::Validate(TargetKg,ToleranceKg,S.ModelParticleMassKg,S.SequentialBatch.B.SourceCount*S.ModelParticleMassKg);
 if(!Reason.IsEmpty())return false;
 AutoSettings.TargetMassBKg=TargetKg;AutoSettings.WeighingToleranceBKg=ToleranceKg;AutoRun.Settings=AutoSettings;return true;
}

bool UProcessControlService::SetAutomaticAdditive(bool Enabled,int32 Supplier,bool Scheduled,double TargetKg,double ToleranceKg,double Delay,FString& Reason)
{
 if(AutoRun.State.Active||State.IsStopping()){Reason=TEXT("진행 중인 배치의 첨가제 설정은 변경할 수 없습니다.");return false;}
 if(!Enabled){AutoSettings.AdditiveEnabled=false;AutoRun.Settings=AutoSettings;Reason.Empty();return true;}
 if(Supplier!=2){Reason=TEXT("현재 첨가제 계량·제동·하류 공기 지원 대상은 C 호퍼입니다. A·B 중복 또는 미검증 D~F는 사용할 수 없습니다.");return false;}
 if(!FMath::IsFinite(Delay)||Delay<0){Reason=TEXT("예약 시각 N은 유한한 0 이상 해석초여야 합니다.");return false;}
 if(Scheduled&&AutoSettings.MixingDurationSeconds>0&&Delay>=AutoSettings.MixingDurationSeconds){Reason=TEXT("예약 시각은 설정 혼합 종료보다 앞서야 합니다.");return false;}
 const auto& S=State.Snapshot;
 if(!State.Connected||S.Validity!=EPlantValueValidity::Valid||!S.SupplyABatch.Valid||!PlantPurgeFor(S,Supplier).Valid||!PlantBrakeFor(S,Supplier).Valid){Reason=TEXT("첨가제 재고·제동·공기 작업자 확인값이 없습니다.");return false;}
 Reason=PlantDose::Validate(TargetKg,ToleranceKg,S.ModelParticleMassKg,S.SequentialBatch.Additive.SourceCount*S.ModelParticleMassKg);
 if(!Reason.IsEmpty()){Reason=TEXT("첨가제 C: ")+Reason;return false;}
 AutoSettings.AdditiveEnabled=true;AutoSettings.AdditiveSupplier=Supplier;AutoSettings.AdditiveScheduled=Scheduled;
 AutoSettings.AdditiveTargetKg=TargetKg;AutoSettings.AdditiveToleranceKg=ToleranceKg;AutoSettings.AdditiveDelaySeconds=Scheduled?Delay:0;
 AutoRun.Settings=AutoSettings;return true;
}

bool UProcessControlService::ResumeAutomaticBatch(){Tick();return AutoRun.Recover(State,FPlatformTime::Seconds());}
FString UProcessControlService::CheckAutomaticBatch(){Tick();auto Why=AutoRun.CanRecover(State,FPlatformTime::Seconds());AutoRun.State.RecoveryReason=Why.IsEmpty()?TEXT("배치 이력 확인 · 명시적인 배치 재개를 기다립니다."):Why;return AutoRun.State.RecoveryReason;}
void UProcessControlService::AbandonAutomaticBatch(){Request(TEXT("plant"),EProcessCommand::StopAll,0);AutoRun.State.RecoveryAbandoned=true;AutoRun.State.RecoveryPending=false;AutoRun.State.RecoveryReason=TEXT("배치 중단 · 이력 보존 · 새 배치는 초기화 필요");}
