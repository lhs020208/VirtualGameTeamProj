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
    WebLeaseId.Empty();WebLeaseUntil=0;
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
        case EProcessCommand::MixerRPM:
            if(State.DispatchMixRun)return A->SetLiveMixerRPM(V,Epoch,State.DispatchMixRun,AutoRun.State.MixingDeadlineSimulationSeconds,State.DispatchDeadline,Revision,Error);
            return A->SetMixerRPM(V,Epoch,Revision,Error);
        case EProcessCommand::Pressure:return A->SetGaugePressure(PlantControl::PressureTarget(Id),V,Epoch,Revision,Error,
            State.StagePressureDispatch&&AutoRun.State.Stage==EProcessAutoStage::Transferring?AutoRun.State.TransferId:0,State.DispatchDeadline,
            State.AutomationDispatch?ProcessPlantBinding::SupplierIndex(Id):INDEX_NONE,
            State.StagePressureDispatch?AutoRun.State.BatchRevision:0,AutoRun.State.ActiveSupplier);
        case EProcessCommand::FinalizeADose:return A->FinalizeADose(AutoRun.State.TargetMassKg,AutoRun.State.WeighingToleranceKg,AutoRun.State.BatchRevision,Epoch,Revision,Error,State.DispatchDeadline);
        case EProcessCommand::BeginAdditive:return A->BeginAdditive(AutoRun.State.AdditiveScheduled,AutoRun.State.AdditiveDelaySeconds,AutoRun.State.BatchRevision,Epoch,Revision,Error,State.DispatchDeadline,AutoRun.State.AdditiveSupplier);
        case EProcessCommand::BeginB:case EProcessCommand::CombineAB:return A->TransitionBatch(C==EProcessCommand::CombineAB,AutoRun.State.BatchRevision,Epoch,Revision,Error,State.DispatchDeadline,AutoRun.State.NextRegular(AutoRun.State.ActiveSupplier));
        case EProcessCommand::RecoverBatch:return A->RecoverBatch(AutoRun.State.BatchRevision,Epoch,State.DispatchRequestId,Revision,Error,State.DispatchDeadline);
        case EProcessCommand::CaptureABatch:return A->CaptureInitialABatch(Epoch,Revision,Error,AutoRun.State.ActiveSupplier);
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
    if(!AutoRun.State.Active||!AutoRun.State.PressureStageToken.IsValid()||!AutoRun.State.ReceiverPressurePending){
        auto* Actor=Source.Get();
        if(Actor)Actor->RevokeTransferPressure();
        if(auto* R=State.History.Find(AutoRun.State.ReceiverPressureRequest);R&&(R->IsPending()||R->Status==EProcessCommandStatus::Unknown)){
            if(Actor&&R->Connection==State.Connection&&Actor->StagePressureRevokedBeforeUpload(R->Epoch,R->Revision)){
                R->Status=EProcessCommandStatus::Rejected;R->Message=TEXT("단계 종료 또는 보호 닫힘 · 작업자 적용 전 압력 요청 취소");
            }else if(R->IsPending()){
                R->Status=EProcessCommandStatus::Unknown;R->Message=TEXT("단계 종료·정지 우선 · 최종 적용 응답 미확인");
            }
        }
    }
    if(WasActive&&!AutoRun.State.Active&&AutoRun.State.Stage==EProcessAutoStage::Cancelled)
        if(auto* A=Source.Get())A->CancelAutomaticControls();
}

FProcessCommandRecord UProcessControlService::Request(const FString& Equipment,EProcessCommand Command,double Value,FGuid Id)
{
    if(Command!=EProcessCommand::StopAll&&!WebDispatch&&WebOwnsControl())return State.Submit(Id.IsValid()?Id:FGuid::NewGuid(),State.Snapshot.Epoch,Equipment,Command,Value,FPlatformTime::Seconds(),Transport(),LocalOperatorReason());
    if(Command==EProcessCommand::Pressure&&Equipment==TEXT("receiver")&&AutoRun.State.Active)
        return RequestTransferReceiverPressure(Id.IsValid()?Id:FGuid::NewGuid(),State.Snapshot.Epoch,AutoRun.State.TransferId,Value);
    if(Command==EProcessCommand::StopAll||Command==EProcessCommand::Pause)CancelAutomatic(TEXT("전체 정지 요청으로 자동 실행과 대기 명령을 취소했습니다. 계산 재개만으로 자동 복원하지 않습니다. 배치 상태 확인 후 별도로 재개하세요."));
    Tick();
    return RequestForEpoch(Id.IsValid()?Id:FGuid::NewGuid(),State.Snapshot.Epoch,Equipment,Command,Value);
}

FProcessCommandRecord UProcessControlService::RequestForEpoch(FGuid Id,int64 Epoch,const FString& Equipment,EProcessCommand Command,double Value)
{
    // Reject/replay before any Tick or automatic cancellation: an old request has no side effects.
    if(State.History.Contains(Id)||Epoch!=State.Snapshot.Epoch)return State.Submit(Id,Epoch,Equipment,Command,Value,FPlatformTime::Seconds(),Transport());
    if(Command!=EProcessCommand::StopAll&&!WebDispatch&&WebOwnsControl())return State.Submit(Id,Epoch,Equipment,Command,Value,FPlatformTime::Seconds(),Transport(),LocalOperatorReason());
    if(Command==EProcessCommand::Pressure&&Equipment==TEXT("receiver")&&AutoRun.State.Active)
        return RequestTransferReceiverPressure(Id,Epoch,AutoRun.State.TransferId,Value);
    if(Command==EProcessCommand::StopAll||Command==EProcessCommand::Pause)CancelAutomatic(TEXT("전체 정지 요청으로 자동 실행과 대기 명령을 취소했습니다."));
    Tick();
    // Snapshot observation can cross a lease deadline or connection/reset boundary.
    // Re-check at dispatch, not only at the network admission point.
    const FString DispatchError=WebDispatch&&Command!=EProcessCommand::StopAll&&!WebOwnsControl()?TEXT("관찰 중 웹 제어권이 만료·해제되었습니다. 명시적으로 다시 가져오세요."):FString();
    auto Result=State.Submit(Id,Epoch,Equipment,Command,Value,FPlatformTime::Seconds(),Transport(),DispatchError);
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
 if(WebOwnsControl()&&!WebDispatch){Reason=LocalOperatorReason();return false;}
 if(AutoRun.State.Active){Reason=TEXT("자동 배치 중 혼합 시간은 변경할 수 없습니다.");return false;}
 AutoRun.Settings=AutoSettings;
 if(!AutoRun.SetMixingDuration(Seconds,Reason))return false;
 AutoSettings=AutoRun.Settings;return true;
}
bool UProcessControlService::StartAutomaticA()
{
 if(WebOwnsControl()&&!WebDispatch)return false;
 Tick();if(WebDispatch&&!WebOwnsControl())return false;AutoRun.Settings=AutoSettings;const bool WasActive=AutoRun.State.Active;
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
 if(WebOwnsControl()&&!WebDispatch){Reason=LocalOperatorReason();return false;}
 if(AutoRun.State.Active||State.IsStopping()){Reason=TEXT("자동 실행·전체 정지 중 배치 설정 변경 불가");return false;}
 if(Mode!=EProcessBatchMode::Full&&Mode!=EProcessBatchMode::TargetMass&&Mode!=EProcessBatchMode::SequentialAB){Reason=TEXT("지원하지 않는 계량 모드");return false;}
 if(!AutoSettings.Entries.IsEmpty()){
  AutoSettings.Entries.Reset();AutoSettings.AdditiveEnabled=false;AutoSettings.AdditiveSupplier=2;AutoSettings.AdditiveScheduled=false;AutoSettings.AdditiveDelaySeconds=0;
 }
 AutoSettings.BatchMode=Mode;AutoRun.Settings=AutoSettings;Reason.Empty();return true;
}
bool UProcessControlService::SetAutomaticTargetMass(double TargetKg,double ToleranceKg,FString& Reason)
{
 if(WebOwnsControl()&&!WebDispatch){Reason=LocalOperatorReason();return false;}
 if(!AutoSettings.Entries.IsEmpty()){Reason=TEXT("호퍼 선택 순차 배치는 레시피의 원료 목록에서 변경하세요.");return false;}
 if(AutoRun.State.Active||State.IsStopping()){Reason=TEXT("자동 실행·전체 정지 중 계량 설정 변경 불가");return false;}
 const auto& S=State.Snapshot;
 if(!State.Connected||!S.SupplyABatch.Valid||S.Validity!=EPlantValueValidity::Valid){Reason=TEXT("실제 A 공급 가능량을 확인할 수 없습니다.");return false;}
 Reason=PlantDose::Validate(TargetKg,ToleranceKg,S.SourceKg(0),S.SupplyABatch.InventorySourceCount*S.SourceKg(0));
 if(!Reason.IsEmpty())return false;
 AutoSettings.TargetMassKg=TargetKg;AutoSettings.WeighingToleranceKg=ToleranceKg;AutoRun.Settings=AutoSettings;return true;
}

bool UProcessControlService::SetAutomaticBTargetMass(double TargetKg,double ToleranceKg,FString& Reason)
{
 if(WebOwnsControl()&&!WebDispatch){Reason=LocalOperatorReason();return false;}
 if(!AutoSettings.Entries.IsEmpty()){Reason=TEXT("호퍼 선택 순차 배치는 레시피의 원료 목록에서 변경하세요.");return false;}
 if(AutoRun.State.Active||State.IsStopping()){Reason=TEXT("자동 실행·전체 정지 중 B 설정 변경 불가");return false;}
 const auto& S=State.Snapshot;
 if(!State.Connected||!S.SupplyABatch.Valid||S.Validity!=EPlantValueValidity::Valid){Reason=TEXT("실제 B 공급 가능량 미확인");return false;}
 Reason=PlantDose::Validate(TargetKg,ToleranceKg,S.SourceKg(1),S.SequentialBatch.B.SourceCount*S.SourceKg(1));
 if(!Reason.IsEmpty())return false;
 AutoSettings.TargetMassBKg=TargetKg;AutoSettings.WeighingToleranceBKg=ToleranceKg;AutoRun.Settings=AutoSettings;return true;
}

bool UProcessControlService::SetAutomaticAdditive(bool Enabled,int32 Supplier,bool Scheduled,double TargetKg,double ToleranceKg,double Delay,FString& Reason)
{
 if(WebOwnsControl()&&!WebDispatch){Reason=LocalOperatorReason();return false;}
 if(!AutoSettings.Entries.IsEmpty()){Reason=TEXT("호퍼 선택 순차 배치는 레시피의 원료 목록에서 변경하세요.");return false;}
 if(AutoRun.State.Active||State.IsStopping()){Reason=TEXT("진행 중인 배치의 첨가제 설정은 변경할 수 없습니다.");return false;}
 if(!Enabled){AutoSettings.AdditiveEnabled=false;AutoRun.Settings=AutoSettings;Reason.Empty();return true;}
 if(Supplier<2||Supplier>=6){Reason=TEXT("A·B와 중복되지 않는 첨가제 호퍼를 선택하세요.");return false;}
 if(!FMath::IsFinite(Delay)||Delay<0){Reason=TEXT("예약 시각 N은 유한한 0 이상 해석초여야 합니다.");return false;}
 if(Scheduled&&AutoSettings.MixingDurationSeconds>0&&Delay>=AutoSettings.MixingDurationSeconds){Reason=TEXT("예약 시각은 설정 혼합 종료보다 앞서야 합니다.");return false;}
 const auto& S=State.Snapshot;
 if(!State.Connected||S.Validity!=EPlantValueValidity::Valid||!S.SupplyABatch.Valid||!PlantPurgeFor(S,Supplier).Valid||!PlantBrakeFor(S,Supplier).Valid){Reason=TEXT("첨가제 재고·제동·공기 작업자 확인값이 없습니다.");return false;}
 Reason=PlantDose::Validate(TargetKg,ToleranceKg,S.SourceKg(Supplier),S.SequentialBatch.Materials.IsValidIndex(Supplier)?S.SequentialBatch.Materials[Supplier].SourceCount*S.SourceKg(Supplier):0);
 if(!Reason.IsEmpty()){Reason=TEXT("첨가제 C: ")+Reason;return false;}
 if(Supplier!=2){
  if(AutoSettings.BatchMode!=EProcessBatchMode::SequentialAB){Reason=TEXT("A·B 순차 모드 또는 호퍼 선택 레시피를 먼저 설정하세요.");return false;}
  auto R=GetAppliedRecipe();R.Mode=EProcessBatchMode::HopperSequence;
  for(int32 I:{0,1,Supplier}){
   auto& E=R.Entries.AddDefaulted_GetRef();E.Supplier=I;E.Additive=I==Supplier;
   E.DisplayName=I==0?R.MaterialA:I==1?R.MaterialB:FString::Printf(TEXT("첨가제 %c"),'A'+I);
   E.TargetKg=I==0?R.TargetAKg:I==1?R.TargetBKg:TargetKg;E.ToleranceKg=I==0?R.ToleranceAKg:I==1?R.ToleranceBKg:ToleranceKg;
  }
  R.AdditiveEnabled=true;R.Scheduled=Scheduled;R.DelaySeconds=Scheduled?Delay:0;return ApplyRecipe(R,Reason);
 }
 AutoSettings.AdditiveEnabled=true;AutoSettings.AdditiveSupplier=Supplier;AutoSettings.AdditiveScheduled=Scheduled;
 AutoSettings.AdditiveTargetKg=TargetKg;AutoSettings.AdditiveToleranceKg=ToleranceKg;AutoSettings.AdditiveDelaySeconds=Scheduled?Delay:0;
 AutoRun.Settings=AutoSettings;return true;
}

bool UProcessControlService::ResumeAutomaticBatch(){if(WebOwnsControl()&&!WebDispatch)return false;Tick();if(WebDispatch&&!WebOwnsControl())return false;return AutoRun.Recover(State,FPlatformTime::Seconds());}
FString UProcessControlService::CheckAutomaticBatch(){Tick();auto Why=AutoRun.CanRecover(State,FPlatformTime::Seconds());AutoRun.State.RecoveryReason=Why.IsEmpty()?TEXT("배치 이력 확인 · 명시적인 배치 재개를 기다립니다."):Why;return AutoRun.State.RecoveryReason;}
void UProcessControlService::AbandonAutomaticBatch(){if(WebOwnsControl()&&!WebDispatch)return;const auto Stop=RequestForEpoch(FGuid::NewGuid(),State.Snapshot.Epoch,TEXT("plant"),EProcessCommand::StopAll,0);if(Stop.Status==EProcessCommandStatus::Rejected)return;AutoRun.State.RecoveryAbandoned=true;AutoRun.State.RecoveryPending=false;AutoRun.State.RecoveryReason=TEXT("배치 중단 · 이력 보존 · 새 배치는 초기화 필요");}

FProcessCommandRecord UProcessControlService::RequestTransferReceiverPressure(FGuid Id,int64 Epoch,int64 TransferId,double Pa,FGuid ExpectedBatch,FString ExpectedLease)
{
    if(State.History.Contains(Id)||Epoch!=State.Snapshot.Epoch)return State.Submit(Id,Epoch,TEXT("receiver"),EProcessCommand::Pressure,Pa,FPlatformTime::Seconds(),Transport());
    if(!WebDispatch&&WebOwnsControl())return State.Submit(Id,Epoch,TEXT("receiver"),EProcessCommand::Pressure,Pa,FPlatformTime::Seconds(),Transport(),LocalOperatorReason());
    if(AutoRun.State.PressureStageToken.IsValid()){
        const auto& A=AutoRun.State;
        return RequestStagePressure(Id,Epoch,WebDispatch?ExpectedBatch:A.RunId,A.PressureStageToken,A.ActiveSupplier,TransferId,TEXT("receiver"),Pa,WebDispatch?&ExpectedLease:nullptr);
    }
    Tick(); // A disconnected or changed source must not accept a late request.
    if(WebDispatch){const auto Why=WebTransferPressureReason(Epoch,ExpectedLease,ExpectedBatch,TransferId,FPlatformTime::Seconds());if(!Why.IsEmpty())return State.Submit(Id,Epoch,TEXT("receiver"),EProcessCommand::Pressure,Pa,FPlatformTime::Seconds(),Transport(),Why);}
    return AutoRun.RequestReceiverPressure(Id,Epoch,TransferId,Pa,State,FPlatformTime::Seconds(),Transport());
}


bool UProcessControlService::WebOwnsControl() const
{
 return !WebLeaseId.IsEmpty()&&WebLeaseEpoch==State.Snapshot.Epoch&&FPlatformTime::Seconds()<WebLeaseUntil&&State.Connected;
}
FString UProcessControlService::LocalOperatorReason() const
{
 return WebOwnsControl()?TEXT("현재 조작 주체: 웹 · 브라우저에서 제어권을 반환하세요. 전체 정지는 양쪽에서 가능합니다."):FString();
}
FProcessCommandRecord UProcessControlService::RequestWeb(FGuid Id,int64 Epoch,const FString& Lease,const FString& Equipment,EProcessCommand Command,double Value)
{
 check(IsInGameThread());
 FString Why;
 if(Command!=EProcessCommand::StopAll&&(!WebOwnsControl()||Lease!=WebLeaseId))Why=TEXT("웹 제어권이 없거나 만료되었습니다. 명시적으로 다시 가져오세요.");
 if(Command!=EProcessCommand::StopAll&&AutoRun.State.Active)Why=TEXT("자동 배치 실행 중 일반 웹 수동 명령은 사용할 수 없습니다. 운전 중 압력과 혼합 RPM은 해당 장비의 전용 적용 버튼을 사용하세요.");
 if(!Why.IsEmpty()||Epoch!=State.Snapshot.Epoch||State.History.Contains(Id))return State.Submit(Id,Epoch,Equipment,Command,Value,FPlatformTime::Seconds(),Transport(),Why);
 TGuardValue<bool> Guard(WebDispatch,true);
 return RequestForEpoch(Id,Epoch,Equipment,Command,Value);
}

FString UProcessControlService::WebTransferPressureReason(int64 Epoch,const FString& Lease,FGuid Batch,int64 TransferId,double Now) const
{
 if(!WebOwnsControl()||Lease!=WebLeaseId)return TEXT("웹 제어권 없음 · 제어권을 명시적으로 가져오세요.");
 if(Epoch!=State.Snapshot.Epoch)return TEXT("이전 초기화 세대의 수취 압력 요청입니다.");
 if(!Batch.IsValid()||Batch!=AutoRun.State.RunId)return TEXT("배치 ID 변경 · 이전 배치 압력 요청을 적용하지 않습니다.");
 if(TransferId<=0||TransferId!=AutoRun.State.TransferId||TransferId!=State.Snapshot.Transfer.Id)return TEXT("이송 ID 변경 · 이전 이송 압력 요청을 적용하지 않습니다.");
 return AutoRun.ReceiverPressureUnavailable(State,Now);
}
FProcessCommandRecord UProcessControlService::RequestWebTransferReceiverPressure(FGuid Id,int64 Epoch,const FString& Lease,FGuid Batch,int64 TransferId,double Pa)
{
 check(IsInGameThread());
 const FString Why=WebTransferPressureReason(Epoch,Lease,Batch,TransferId,FPlatformTime::Seconds());
 if(State.History.Contains(Id)||!Why.IsEmpty())return State.Submit(Id,Epoch,TEXT("receiver"),EProcessCommand::Pressure,Pa,FPlatformTime::Seconds(),Transport(),Why);
 TGuardValue<bool> Guard(WebDispatch,true);
 return RequestTransferReceiverPressure(Id,Epoch,TransferId,Pa,Batch,Lease);
}

FProcessCommandRecord UProcessControlService::RequestMixSpeed(FGuid Id,int64 Epoch,FGuid Batch,int64 MixRun,double RPM,const FString* WebLease)
{
 check(IsInGameThread());
 if(const auto* Previous=State.History.Find(Id)){
  if(Previous->SpeedBatch!=Batch||Previous->SpeedRun!=MixRun){
   auto R=*Previous;R.Status=EProcessCommandStatus::Rejected;R.Message=TEXT("같은 요청 번호의 배치 또는 혼합 운전이 다릅니다.");return R;
  }
  return State.Submit(Id,Epoch,TEXT("mixer"),EProcessCommand::MixerRPM,RPM,FPlatformTime::Seconds(),Transport());
 }
 Tick();
 const double Now=FPlatformTime::Seconds();
 auto& A=AutoRun.State;
 FString Why=AutoRun.MixSpeedReason(State,Now);
 if(WebLease&&(!WebOwnsControl()||*WebLease!=WebLeaseId))Why=TEXT("웹 제어권을 확인하세요.");
 else if(!WebLease&&WebOwnsControl())Why=LocalOperatorReason();
 else if(!Batch.IsValid()||A.RunId!=Batch||A.Epoch!=Epoch||MixRun<=0||A.ConfirmedMixRun!=MixRun)
  Why=TEXT("대상 배치 또는 혼합 운전이 변경되었습니다. 현재 혼합기에서 다시 입력하세요.");
 else if(!FMath::IsFinite(RPM)||RPM<=0||RPM>PlantControl::MaxRPM||float(RPM)==0.f)
  Why=TEXT("0 초과 30 이하 RPM을 입력하세요. 정지하려면 전체 정지를 사용하세요.");
 TGuardValue<bool> Automatic(State.AutomationDispatch,true);
 TGuardValue<int64> Scope(State.DispatchMixRun,MixRun);
 auto R=State.Submit(Id,Epoch,TEXT("mixer"),EProcessCommand::MixerRPM,RPM,Now,Transport(),Why);
 R.SpeedBatch=Batch;State.History.FindChecked(Id).SpeedBatch=Batch;
 if(R.IsPending())A.SpeedRequest=Id;
 return R;
}

FProcessCommandRecord UProcessControlService::RequestStagePressure(FGuid Id,int64 Epoch,FGuid Batch,FGuid Stage,int32 Supplier,int64 TransferId,const FString& Equipment,double Pa,const FString* WebLease)
{
 check(IsInGameThread());
 if(const auto* Existing=State.History.Find(Id)){
  if(Existing->PressureBatch!=Batch||Existing->PressureStage!=Stage||Existing->PressureSupplier!=Supplier||Existing->PressureTransfer!=TransferId){
   auto R=*Existing;R.Status=EProcessCommandStatus::Rejected;R.Message=TEXT("같은 요청 ID의 압력 대상 단계가 달라졌습니다.");return R;
  }
  return State.Submit(Id,Epoch,Equipment,EProcessCommand::Pressure,Pa,FPlatformTime::Seconds(),Transport());
 }
 Tick();
 const auto& A=AutoRun.State;
 FString Why;
 if(WebLease&&(!WebOwnsControl()||*WebLease!=WebLeaseId))Why=TEXT("웹 제어권을 다시 확인하세요.");
 else if(!WebLease&&WebOwnsControl())Why=LocalOperatorReason();
 else if(!A.Active||!Stage.IsValid()||Stage!=A.PressureStageToken||Batch!=A.RunId||Epoch!=A.Epoch||Supplier!=A.ActiveSupplier||
         TransferId!=(A.Stage==EProcessAutoStage::Transferring?A.TransferId:0))Why=TEXT("압력 요청의 배치·공급원·단계가 변경되었습니다. 현재 구간에서 다시 입력하세요.");
 if(!Why.IsEmpty())return State.Submit(Id,Epoch,Equipment,EProcessCommand::Pressure,Pa,FPlatformTime::Seconds(),Transport(),Why);
 return AutoRun.RequestStagePressure(Id,Epoch,TransferId,Pa,State,FPlatformTime::Seconds(),Transport(),Equipment);
}
