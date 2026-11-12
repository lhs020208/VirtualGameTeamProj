#include "ProcessControlTypes.h"
#include "ProcessPlantBinding.h"

namespace
{
bool IsSupplier(const FString& Id) { return ProcessPlantBinding::SupplierIndex(Id) != INDEX_NONE; }
bool IsValve(const FString& Id) { return IsSupplier(Id) || Id == TEXT("weigher") || Id == TEXT("receiver"); }
bool IsSafety(EProcessCommand C, double V) { return C == EProcessCommand::StopAll || ((C == EProcessCommand::Valve || C == EProcessCommand::MixerEnabled) && V == 0); }
}

void FProcessControlMachine::InvalidatePending(const FString& Reason, EProcessCommandStatus Status)
{
    for (auto& Pair : History) if (Pair.Value.IsPending()) { Pair.Value.Status = Status; Pair.Value.Message = Reason; }
}

void FProcessControlMachine::Connect(const FPlantSimulationSnapshot& State)
{
    InvalidatePending(TEXT("연결이 변경되었습니다. 이전 요청 결과는 미확인입니다."), EProcessCommandStatus::Unknown);
    ++Connection; Snapshot = State; Connected = true; ManualGranted = false;
    StopStage = EProcessStopStage::None; StopRequest.Invalidate();
    FreezeState = EProcessFreezeState::None; StopFailureEquipment.Empty(); StopFailureReason.Empty();
}

void FProcessControlMachine::Disconnect(const FString& Reason)
{
    InvalidatePending(Reason, EProcessCommandStatus::Unknown);
    if (IsStopping()) StopStage = EProcessStopStage::Failed;
    if (FreezeState != EProcessFreezeState::None)
        SetFreezeResult(EProcessFreezeState::Unconfirmed, TEXT("연결 해제 · 현재 계산 동결 여부 미확인"));
    Connected = false; ManualGranted = false;
}

bool FProcessControlMachine::Fresh(double Now) const
{
    return Connected && Snapshot.Validity == EPlantValueValidity::Valid && Snapshot.PublishedWallSeconds > 0 &&
        Now >= Snapshot.PublishedWallSeconds && Now - Snapshot.PublishedWallSeconds <= 2.0;
}

bool FProcessControlMachine::UsesStopSequence(EProcessCommand C, double V) const
{
    return C == EProcessCommand::StopAll || (C == EProcessCommand::Pause && PausePolicy == EProcessPausePolicy::CloseThenPause) ||
        (IsSafety(C,V) && Snapshot.Execution == EPlantExecutionState::Paused && PausedClosePolicy == EProcessPausedClosePolicy::StopAll);
}

FString FProcessControlMachine::UnavailableReason(const FString& Id, EProcessCommand C, double V, double Now) const
{
    if (!FMath::IsFinite(V)) return TEXT("유한한 숫자를 입력하세요.");
    if ((C == EProcessCommand::Valve && !IsValve(Id)) ||
        ((C == EProcessCommand::MixerEnabled || C == EProcessCommand::MixerRPM) && Id != TEXT("mixer")) ||
        ((C == EProcessCommand::StopAll || C == EProcessCommand::Resume || C == EProcessCommand::Pause) && Id != TEXT("plant")))
        return TEXT("이 장비에서 지원하지 않는 명령입니다.");
    if(C == EProcessCommand::Valve && IsSupplier(Id) && !Snapshot.RequestedSuppliers.IsValidIndex(ProcessPlantBinding::SupplierIndex(Id)))
        return TEXT("이 해석 구성에서는 시뮬레이션 미구현입니다.");
    if ((C == EProcessCommand::Valve || C == EProcessCommand::MixerEnabled) && V != 0 && V != 1)
        return TEXT("열림/닫힘 또는 시작/정지 값만 사용할 수 있습니다.");
    if (C == EProcessCommand::MixerRPM && (V < PlantControl::MinRPM || V > PlantControl::MaxRPM))
        return TEXT("목표 회전속도는 0~30 rpm입니다. 범위를 벗어난 값은 적용하지 않습니다.");
    if (!Connected) return TEXT("시뮬레이션에 연결되지 않았습니다.");
    if (Snapshot.Validity == EPlantValueValidity::Error || Snapshot.Execution == EPlantExecutionState::Fault)
        return TEXT("계산 오류입니다. 복구·초기화 후 다시 연결해야 합니다.");
    if (!Fresh(Now)) return TEXT("상태가 오래되었거나 준비 중입니다. 최신 확인값을 기다리세요.");
    if (!ProcessPlantBinding::SupportedCount(Snapshot.RequestedSuppliers.Num())) return TEXT("검증되지 않은 공급기 구성입니다.");
    if (UsesStopSequence(C,V))
    {
        if(!FMath::IsFinite(StopTimeoutSeconds)||StopTimeoutSeconds<=0)return TEXT("전체 정지 제한 시간은 유한한 양수여야 합니다.");
        if(!FMath::IsFinite(CommandTimeoutSeconds)||CommandTimeoutSeconds<=0)return TEXT("작업자 응답 제한 시간은 유한한 양수여야 합니다.");
        if (TimeoutPolicy == EProcessTimeoutPolicy::Unspecified) return TEXT("닫힘 실패 시 계산 처리 정책을 먼저 설정해야 합니다.");
    }
    if (C == EProcessCommand::StopAll) return IsStopping() ? TEXT("전체 정지를 확인하고 있습니다.") : FString();
    if (IsStopping() && !IsSafety(C,V)) return TEXT("전체 정지 확인 중에는 새 운전·개방 요청을 보낼 수 없습니다.");
    if (IsSafety(C,V))
    {
        FProcessCommandRecord Test; Test.Command=C; Test.EquipmentId=Id; Test.Value=V;
        if (Snapshot.Execution == EPlantExecutionState::Paused && !Matches(Test))
        {
            if (PausedClosePolicy != EProcessPausedClosePolicy::StopAll)
                return TEXT("계산 일시정지 중에는 닫힘을 처리할 수 없습니다. 전체 정지를 선택하세요.");
            if (TimeoutPolicy == EProcessTimeoutPolicy::Unspecified) return TEXT("정지 실패 정책 설정이 필요합니다.");
        }
        return FString(); // Closing/stop never waits behind another device command.
    }
    if (!ManualGranted) return TEXT("먼저 ‘수동 조작 · 전체 정지’를 선택해 정지 완료를 확인하세요.");
    // Manual pause has the same priority as full stop, including over pending openings/RPM changes.
    if (C == EProcessCommand::Pause && UsesStopSequence(C,V)) return FString();
    if(!FMath::IsFinite(CommandTimeoutSeconds)||CommandTimeoutSeconds<=0)return TEXT("명령 제한 시간은 유한한 양수여야 합니다.");
    if (C == EProcessCommand::Pause && PausePolicy == EProcessPausePolicy::Unspecified) return TEXT("계산 일시정지 정책 설정이 필요합니다.");
    if (C == EProcessCommand::Resume && Snapshot.Execution != EPlantExecutionState::Paused && Snapshot.Execution != EPlantExecutionState::Running)
        return TEXT("계산 실행 상태 확인을 기다리세요.");
    if ((C == EProcessCommand::Valve || C == EProcessCommand::MixerEnabled) &&
        (Snapshot.Execution != EPlantExecutionState::Running || !Snapshot.RequestedRunning))
        return TEXT("계산이 일시정지 중입니다. ‘수동 운전 시작 · 계산 재개’를 먼저 누르세요.");
    for (const auto& Pair : History)
        if (Pair.Value.IsPending() && (Pair.Value.EquipmentId == Id || Pair.Value.EquipmentId == TEXT("plant")))
            return TEXT("이 장비의 이전 요청을 확인 중입니다. 닫기·전체 정지는 사용할 수 있습니다.");
    return FString();
}

bool FProcessControlMachine::Matches(const FProcessCommandRecord& R) const
{
    if (R.Command == EProcessCommand::Valve)
    {
        const bool Want=R.Value!=0;
        if (IsSupplier(R.EquipmentId))
        {
            const int32 I=R.EquipmentId[7]-TCHAR('a');
            return Snapshot.RequestedSuppliers.IsValidIndex(I) && Snapshot.RequestedSuppliers[I]==Want &&
                ((Snapshot.AppliedSupplierMask & (1<<I))!=0)==Want;
        }
        if (R.EquipmentId==TEXT("weigher")) return Snapshot.RequestedHopperOpen==Want && Snapshot.AppliedHopperOpen==Want;
        if (R.EquipmentId==TEXT("receiver")) return Snapshot.RequestedReceiverOpen==Want && Snapshot.AppliedReceiverOpen==Want;
    }
    if (R.Command==EProcessCommand::MixerRPM)
        return Snapshot.WorkerTargetRPM==R.Value && Snapshot.RequestedRPM==R.Value &&
            (!Snapshot.WorkerMixerEnabled || (Snapshot.AppliedRPMValid && Snapshot.AppliedRPM==double(float(R.Value))));
    if (R.Command==EProcessCommand::MixerEnabled)
        return Snapshot.RequestedMixerEnabled==(R.Value!=0) && Snapshot.WorkerMixerEnabled==(R.Value!=0) && Snapshot.AppliedRPMValid &&
            Snapshot.AppliedRPM==(R.Value!=0 ? double(float(Snapshot.WorkerTargetRPM)) : 0.0);
    if (R.Command==EProcessCommand::Resume) return Snapshot.Execution==EPlantExecutionState::Running && Snapshot.RequestedRunning;
    if (R.Command==EProcessCommand::Pause) return Snapshot.Execution==EPlantExecutionState::Paused && !Snapshot.RequestedRunning;
    return false;
}

bool FProcessControlMachine::AllClosed() const
{
    if (!ProcessPlantBinding::SupportedCount(Snapshot.RequestedSuppliers.Num()) || Snapshot.AppliedSupplierMask!=0 || Snapshot.AppliedHopperOpen || Snapshot.AppliedReceiverOpen ||
        Snapshot.RequestedHopperOpen || Snapshot.RequestedReceiverOpen || Snapshot.RequestedMixerEnabled || Snapshot.WorkerMixerEnabled ||
        !Snapshot.AppliedRPMValid || Snapshot.AppliedRPM!=0) return false;
    for (bool Open:Snapshot.RequestedSuppliers) if (Open) return false;
    return true;
}

FProcessCommandRecord FProcessControlMachine::Submit(FGuid Id, int64 Epoch, const FString& Equipment, EProcessCommand Command, double Value,
    double Now, const FProcessControlTransport& Send)
{
    if (const auto* Existing=History.Find(Id))
    {
        if (Existing->Epoch==Epoch && Existing->Connection==Connection && Existing->EquipmentId==Equipment && Existing->Command==Command && Existing->Value==Value)
            return *Existing; // Idempotent replay, including terminal and timed-out requests.
        auto R=*Existing; R.Status=EProcessCommandStatus::Rejected; R.Message=TEXT("같은 요청 번호를 다른 내용이나 연결에서 다시 사용할 수 없습니다."); return R;
    }
    FProcessCommandRecord R;
    R.RequestId=Id; R.Epoch=Epoch; R.Connection=Connection; R.EquipmentId=Equipment; R.Command=Command; R.Value=Value;
    R.AcceptedAt=Now; R.Deadline=Now+FMath::Max(0.1,CommandTimeoutSeconds); R.BaselineSequence=Snapshot.Sequence;
    FString Reason = !Id.IsValid() ? TEXT("요청 번호가 필요합니다.") : Epoch!=Snapshot.Epoch ? TEXT("초기화 세대가 변경되었습니다. 새 상태에서 다시 요청하세요.") : UnavailableReason(Equipment,Command,Value,Now);
    if (!Reason.IsEmpty()) { R.Status=EProcessCommandStatus::Rejected; R.Message=Reason; History.Add(Id,R); LastRequest=Id; return R; }
    const bool StopSequence=UsesStopSequence(Command,Value);
    if (StopSequence)
    {
        InvalidatePending(TEXT("전체 정지가 우선 처리됩니다. 이전 요청의 최종 적용 여부는 미확인입니다."),EProcessCommandStatus::Unknown);
        ManualGranted=false; StopStage=EProcessStopStage::Requested; StopRequest=Id; R.UsesStopSequence=true;
        FreezeState=EProcessFreezeState::None;StopFailureEquipment.Empty();StopFailureReason.Empty();
        StopDeadline=Now+FMath::Max(0.1,StopTimeoutSeconds); R.Deadline=StopDeadline;
    }
    else if (IsSafety(Command,Value))
    {
        for(auto& Pair:History) if(Pair.Value.IsPending() && !Pair.Value.UsesStopSequence && Pair.Value.EquipmentId==Equipment)
        { Pair.Value.Status=EProcessCommandStatus::Unknown; Pair.Value.Message=TEXT("닫기·정지 요청이 우선 처리되어 이전 요청 결과는 미확인입니다."); }
    }
    const bool MailboxConfirmed=Snapshot.ProcessedControlRevision>=Snapshot.RequestedControlRevision;
    if (StopSequence && MailboxConfirmed && AllClosed() && Snapshot.Execution==EPlantExecutionState::Paused && !Snapshot.RequestedRunning)
    {
        StopStage=EProcessStopStage::Complete;ManualGranted=true;
        R.Status=EProcessCommandStatus::Applied;R.Message=TEXT("이미 시뮬레이션 정지 완료 · 최신 밸브·RPM·작업자 일시정지 확인값 기준");
    }
    else if (!StopSequence && MailboxConfirmed && Matches(R))
    {
        R.Status=EProcessCommandStatus::Applied;
        R.Message=Command==EProcessCommand::Valve ? (Value==0?TEXT("이미 닫힘 · 최신 적용값 확인"):TEXT("이미 열림 · 최신 적용값 확인")) : TEXT("이미 요청한 상태입니다 · 최신 확인값 기준");
    }
    else if (!Send(StopSequence?EProcessCommand::StopAll:Command,Equipment,Value,Epoch,R.Revision,Reason))
    {
        R.Status=EProcessCommandStatus::Failed; R.Message=Reason;
        if(StopSequence)StopStage=EProcessStopStage::Failed;
    }
    else
    {
        R.Status=EProcessCommandStatus::Processing;
        R.Message=StopSequence?TEXT("정지 요청 중 · 닫힘 처리를 위해 계산을 유지/재개한 뒤 다시 일시정지합니다."):TEXT("접수됨 · 작업자 적용 확인 중");
        if(StopSequence)StopRevision=R.Revision;
    }
    History.Add(Id,R);LastRequest=Id;return R;
}

void FProcessControlMachine::FailStop(const FString& Reason, double Now, const FProcessControlTransport& Send)
{
    StopStage=EProcessStopStage::Failed; ManualGranted=false;
    StopFailureReason=Reason;StopFailureEquipment=UnconfirmedEquipment();
    SetFreezeResult(EProcessFreezeState::None, TEXT("전체 정지 다시 시도로 닫힘 확인을 재시도하세요."));
    if(TimeoutPolicy==EProcessTimeoutPolicy::Freeze)
    {
        FreezeBaselineSequence=Snapshot.Sequence;FreezeDeadline=Now+CommandTimeoutSeconds;
        FString Error;
        if(Fresh(Now) && Send(EProcessCommand::Pause,TEXT("plant"),0,Snapshot.Epoch,FreezeRevision,Error))
            SetFreezeResult(EProcessFreezeState::Awaiting, TEXT("계산 동결 요청 · 작업자 일시정지 응답 확인 중"));
        else SetFreezeResult(EProcessFreezeState::Unconfirmed, TEXT("계산 동결 요청 실패 · 동결 미확인: ")+Error);
    }
}

void FProcessControlMachine::SetFreezeResult(EProcessFreezeState Result, const FString& Message)
{
    FreezeState=Result;
    if(auto* R=History.Find(StopRequest))
    {R->Status=EProcessCommandStatus::Failed;R->Message=StopFailureReason+TEXT(" 정지 실패 · 미확인: ")+StopFailureEquipment+TEXT(" · ")+Message;}
}

void FProcessControlMachine::Observe(const FPlantSimulationSnapshot& State, double Now, const FProcessControlTransport& Send)
{
    if(!Connected)return;
    if(State.Epoch!=Snapshot.Epoch){Connect(State);return;}
    if(State.Sequence<Snapshot.Sequence)return;
    Snapshot=State;
    if(!Fresh(Now))
    {
        const bool Error=State.Validity==EPlantValueValidity::Error || State.Execution==EPlantExecutionState::Fault;
        InvalidatePending(Error?TEXT("작업자 오류 · 복구·초기화가 필요합니다."):TEXT("새 상태를 확인할 수 없습니다. 요청 결과 미확인입니다."),Error?EProcessCommandStatus::Failed:EProcessCommandStatus::Unknown);
        if(IsStopping())
        {
            StopStage=EProcessStopStage::Failed;
            StopFailureReason=Error?TEXT("작업자 오류로 전체 정지를 확인할 수 없습니다."):TEXT("상태가 만료되어 전체 정지를 확인할 수 없습니다.");
            StopFailureEquipment=UnconfirmedEquipment();
            if(TimeoutPolicy==EProcessTimeoutPolicy::Freeze)FreezeState=EProcessFreezeState::Unconfirmed;
        }
        if(FreezeState!=EProcessFreezeState::None)
            SetFreezeResult(EProcessFreezeState::Unconfirmed, Error?TEXT("작업자 오류 · 동결 미확인 · 복구·초기화 필요"):TEXT("상태 만료 · 현재 계산 동결 여부 미확인"));
        ManualGranted=false;return;
    }
    if(FreezeState==EProcessFreezeState::Awaiting)
    {
        // A successful send or an old paused snapshot is not a worker acknowledgement.
        if(State.Sequence>FreezeBaselineSequence && State.ProcessedControlRevision>=FreezeRevision &&
            State.Execution==EPlantExecutionState::Paused && !State.RequestedRunning)
            SetFreezeResult(EProcessFreezeState::Confirmed, TEXT("계산 동결 확인 · 작업자 일시정지 응답 수신 · 전체 정지 다시 시도 필요"));
        else if(Now>=FreezeDeadline)
            SetFreezeResult(EProcessFreezeState::Unconfirmed, TEXT("작업자 일시정지 응답 시간 초과 · 계산 동결 미확인 · 복구 또는 전체 정지 재시도 필요"));
    }
    if(IsStopping())
    {
        if(Now>=StopDeadline)FailStop(TEXT("전체 정지 제한 시간을 초과했습니다."),Now,Send);
        else if(StopStage==EProcessStopStage::Requested || StopStage==EProcessStopStage::Closing)
        {
            StopStage=EProcessStopStage::Closing;
            auto* R=History.Find(StopRequest);
            if(R)R->Message=TEXT("밸브 닫힘 대기 · ")+UnconfirmedEquipment();
            if(R && State.Sequence>R->BaselineSequence && State.ProcessedControlRevision>=StopRevision && AllClosed())
            {
                FString Error;
                if(Send(EProcessCommand::Pause,TEXT("plant"),0,State.Epoch,PauseRevision,Error))
                { StopStage=EProcessStopStage::Pausing;R->Message=TEXT("모든 밸브 닫힘·계산 적용 RPM 0 확인 · 계산 정지 확인 중"); }
                else FailStop(TEXT("계산 일시정지 요청 실패: ")+Error,Now,Send);
            }
        }
        else if(StopStage==EProcessStopStage::Pausing && State.ProcessedControlRevision>=PauseRevision &&
            State.Execution==EPlantExecutionState::Paused && !State.RequestedRunning && AllClosed())
        {
            StopStage=EProcessStopStage::Complete;ManualGranted=true;
            if(auto* R=History.Find(StopRequest)){R->Status=EProcessCommandStatus::Applied;R->Message=FString::Printf(TEXT("시뮬레이션 정지 완료 · 제어 가능한 %d개 밸브 닫힘, 계산 적용 RPM 0, 작업자 일시정지 확인"),Snapshot.RequestedSuppliers.Num()+2);}
        }
    }
    for(auto& Pair:History)
    {
        auto& R=Pair.Value;
        if(!R.IsPending() || R.UsesStopSequence)continue;
        if(State.Sequence>R.BaselineSequence && State.ProcessedControlRevision>=R.Revision && Matches(R))
        {
            R.Status=EProcessCommandStatus::Applied;
            R.Message=R.Command==EProcessCommand::MixerRPM && !State.WorkerMixerEnabled ?
                TEXT("목표 회전속도 저장 확인 · 혼합기는 정지 상태를 유지합니다."):TEXT("적용 완료 · 요청 이후 작업자 확인값 기준");
        }
        else if(Now>=R.Deadline)
        {R.Status=EProcessCommandStatus::Unknown;R.Message=TEXT("제한 시간 안에 적용을 확인하지 못했습니다. 요청은 자동 취소되지 않습니다. 최신 상태 확인 또는 전체 정지가 필요합니다.");ManualGranted=false;}
    }
}

FString FProcessControlMachine::UnconfirmedEquipment() const
{
    TArray<FString> Names;
    if(!ProcessPlantBinding::SupportedCount(Snapshot.RequestedSuppliers.Num()))Names.Add(TEXT("공급기 구성 미확인"));
    for(int32 I=0;I<Snapshot.RequestedSuppliers.Num();++I)if(Snapshot.RequestedSuppliers[I]||(Snapshot.AppliedSupplierMask&(1<<I)))Names.Add(FString::Printf(TEXT("공급 %c"),TCHAR('A'+I)));
    if(Snapshot.AppliedHopperOpen||Snapshot.RequestedHopperOpen)Names.Add(TEXT("칭량호퍼 출구"));
    if(Snapshot.AppliedReceiverOpen||Snapshot.RequestedReceiverOpen)Names.Add(TEXT("수취호퍼 출구"));
    if(!Snapshot.AppliedRPMValid||Snapshot.AppliedRPM!=0||Snapshot.WorkerMixerEnabled||Snapshot.RequestedMixerEnabled)Names.Add(TEXT("혼합기 계산 적용 RPM 0"));
    if(Names.IsEmpty())Names.Add(TEXT("작업자 일시정지 확인"));
    return FString::Join(Names,TEXT(", "));
}

FString FProcessControlMachine::StopDescription() const
{
    switch(StopStage){
    case EProcessStopStage::Requested:return TEXT("정지 요청 중 · 닫힘 처리에 필요한 계산 유지/재개");
    case EProcessStopStage::Closing:return TEXT("밸브 닫힘 대기 · 계산 유지/재개 후 다시 일시정지");
    case EProcessStopStage::Pausing:return TEXT("계산 정지 확인 중");
    case EProcessStopStage::Complete:
        if(!ManualGranted)return TEXT("상태 재확인 필요 · 수동 운전 허용 보류");
        if(Snapshot.Execution==EPlantExecutionState::Running)return TEXT("수동 운전 중");
        if(Snapshot.Execution==EPlantExecutionState::PauseRequested)return TEXT("계산 일시정지 확인 중");
        if(Snapshot.Execution==EPlantExecutionState::ResumeRequested)return TEXT("계산 재개 확인 중");
        return AllClosed()?TEXT("시뮬레이션 정지 완료 · 수동 조작 허용"):TEXT("수동 모드 · 계산만 일시정지 · 장비 요청 유지");
    case EProcessStopStage::Failed:
        if(FreezeState==EProcessFreezeState::Awaiting)return TEXT("정지 실패 · 계산 동결 응답 확인 중");
        if(FreezeState==EProcessFreezeState::Confirmed)return TEXT("정지 실패 · 계산 동결 확인 · 전체 정지 다시 시도 필요");
        if(FreezeState==EProcessFreezeState::Unconfirmed)return TEXT("정지 실패 · 계산 동결 미확인 · 수동 운전 차단");
        return TEXT("정지 실패 · 수동 운전 허용 보류");
    default:return TEXT("장비 조회 중 · 수동 조작 전 전체 정지 확인 필요");}
}

FString FProcessControlMachine::StatusText(EProcessCommandStatus S)
{
    switch(S){case EProcessCommandStatus::Accepted:return TEXT("접수");case EProcessCommandStatus::Processing:return TEXT("처리 중");
    case EProcessCommandStatus::Applied:return TEXT("적용 완료");case EProcessCommandStatus::Rejected:return TEXT("거부");
    case EProcessCommandStatus::Failed:return TEXT("실패");default:return TEXT("결과 미확인");}
}
