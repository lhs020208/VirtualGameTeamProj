#include "ProcessController.h"

namespace ProcessUI
{
static double Amount(const TMap<FString,double>& Ledger, const FString& Id) { const double* P=Ledger.Find(Id); return P ? *P : 0; }
static bool Positive(double V) { return FMath::IsFinite(V) && V>0; }
TArray<FString> FController::Validate(const FRecipe& R) const
{
    TArray<FString> Errors;
    auto Add=[&](const FString& E){ Errors.Add(E); };
    const FBounds B=Provider->Bounds();
    if(R.Version!=1) Add(TEXT("지원하지 않는 저장 버전"));
    if(R.Name.TrimStartAndEnd().IsEmpty()) Add(TEXT("배치 이름 필요"));
    if(!Positive(R.RPM)||!Positive(R.MixSeconds)) Add(TEXT("목표 RPM과 혼합 시간은 유효한 양수여야 합니다."));
    if(!B.Valid) Add(TEXT("공급원의 용량·준비 시간 상한 미지원: 자동 실행 불가"));
    TSet<FString> Ids;
    for(const auto& H:R.Hoppers)
    {
        if(H.Id.IsEmpty()||Ids.Contains(H.Id)||H.Id==TEXT("weigher")||H.Id==TEXT("receiver")||H.Id==TEXT("mixer")) Add(TEXT("공급 호퍼 ID 누락·중복 또는 예약된 장비 ID 사용"));
        Ids.Add(H.Id);
        if(H.Material.TrimStartAndEnd().IsEmpty()||!Positive(H.DiameterM)||!Positive(H.RateKgS)) Add(H.Id+TEXT(": 물질·입자 지름·공급 속도를 입력하세요."));
    }
    if(R.Steps.IsEmpty()) Add(TEXT("시퀀스가 비어 있습니다."));
    Ids.Empty();
    bool Started=false; double Accumulated=0,Total=0,Preparation=0,Available=0;
    for(int32 I=0;I<R.Steps.Num();++I)
    {
        const auto& S=R.Steps[I];
        FString Prefix=FString::Printf(TEXT("단계 %d (%s): "),I+1,*S.Name);
        if(S.Id.IsEmpty()||Ids.Contains(S.Id)) Add(Prefix+TEXT("단계 ID 누락/중복"));
        Ids.Add(S.Id);
        if(S.Kind==EStepKind::WeighTransfer)
        {
            const FHopper* H=R.Hoppers.FindByPredicate([&](const FHopper& X){return X.Id==S.HopperId;});
            if(!H) Add(Prefix+TEXT("공급 호퍼를 찾을 수 없습니다."));
            if(!Positive(S.TargetKg)) Add(Prefix+TEXT("목표 질량은 유효한 양수여야 합니다."));
            if(B.Valid && S.TargetKg>B.WeighCapacityKg) Add(Prefix+TEXT("칭량호퍼 용량 초과"));
            Accumulated+=S.TargetKg; Total+=S.TargetKg;
            if(H&&Positive(H->RateKgS)&&B.Valid) Preparation+=S.TargetKg/H->RateKgS+S.TargetKg/B.TransferKgS+2*B.SettleSeconds+6*B.CommandSeconds;
        }
        else
        {
            if(!Positive(Accumulated)) Add(Prefix+TEXT("앞선 개별 칭량·수취 이송이 필요합니다."));
            if(B.Valid&&Accumulated>B.ReceiverCapacityKg) Add(Prefix+TEXT("수취호퍼 용량 초과"));
            if(!Started && S.Timing!=ETiming::Initial) Add(Prefix+TEXT("첫 믹서 투입은 초기 투입이어야 합니다. 예약 0초와 다릅니다."));
            if(Started && S.Timing==ETiming::Initial) Add(Prefix+TEXT("혼합 시작 이후에는 예약 조건을 지정하세요."));
            if(S.Timing==ETiming::AfterMixing)
            {
                if(!FMath::IsFinite(S.AtSeconds)||S.AtSeconds<0||S.AtSeconds>=R.MixSeconds) Add(Prefix+TEXT("예약 시각은 0 이상, 혼합 종료 전이어야 합니다."));
                if(B.Valid&&Available+Preparation>S.AtSeconds) Add(Prefix+FString::Printf(TEXT("수취호퍼 점유/준비 시간 충돌: 최초 준비 가능 %.2f초, 예약 %.2f초"),Available+Preparation,S.AtSeconds));
                if(B.Valid&&S.AtSeconds+Accumulated/B.TransferKgS+B.SettleSeconds+2*B.CommandSeconds>R.MixSeconds) Add(Prefix+TEXT("예약 배출이 혼합 종료 이전에 끝나지 않습니다."));
                Available=S.AtSeconds+(B.Valid?Accumulated/B.TransferKgS+B.SettleSeconds+2*B.CommandSeconds:0);
            }
            else Available=0;
            Started=true; Accumulated=0; Preparation=0;
        }
    }
    if(!Started||Accumulated>0) Add(TEXT("시퀀스 마지막에 믹서 투입 단계가 필요합니다."));
    if(B.Valid&&Total>B.MixerCapacityKg) Add(TEXT("믹서 용량 초과"));
    return Errors;
}
void FController::Send(ECommandKind Kind,const FString& Device,double Target,const FString& Step)
{
    FCommand C; C.Request=NextRequest++; C.Generation=(Kind==ECommandKind::StopAll||Kind==ECommandKind::CloseValve)?Snapshot().Generation:Generation; C.Batch=Batch; C.Step=Step; C.Device=Device; C.Kind=Kind; C.Target=Target;
    if(Kind==ECommandKind::Feed)
        if(const FHopper* H=Recipe.Hoppers.FindByPredicate([&](const FHopper& X){return X.Id==Device;})) { C.Rate=H->RateKgS; C.DiameterM=H->DiameterM; C.Material=H->Material; }
    ActiveRequest=C.Request;if(Kind==ECommandKind::StopAll){LastStopRequest=C.Request;LastStopGeneration=C.Generation;} Provider->Submit(C);
}
bool FController::CommandApplied() const
{
    for(const auto& R:Snapshot().Receipts) if(R.Request==ActiveRequest&&R.Generation==Generation) return R.Result==ECommandResult::Applied;
    return false;
}
bool FController::ConditionsCurrent() const
{
    return Snapshot().ConditionsValidity==EValidity::Valid && Snapshot().ConditionsRequest==ActiveRequest && Snapshot().Generation==Generation;
}
bool FController::Start()
{
    if(!Editable()) return false;
    auto Errors=Validate(Recipe);
    if(!Errors.IsEmpty()) { Message=FString::Join(Errors,TEXT("\n")); return false; }
    if(!Snapshot().CanAutomate||!Snapshot().LedgerValid||Snapshot().WrongMaterial||!ConfirmedSafe()) { Message=TEXT("공급원 초기 상태·정지·밸브 닫힘 확인 필요"); return false; }
    for(const auto& D:Snapshot().Devices) if(D.Mass.Valid()&&D.Mass.Value>1e-8) { Message=TEXT("장비 잔류량이 있습니다. 초기화 확인 필요"); return false; }
    for(const FString Id:{FString(TEXT("weigher")),FString(TEXT("receiver")),FString(TEXT("mixer"))})
    {const auto* D=Snapshot().Devices.FindByPredicate([&](const FDevice& X){return X.Id==Id;});if(!D||!D->Mass.Valid()){Message=TEXT("필수 장비의 잔류 질량 데이터 없음: 시작 불가");return false;}}
    Generation=Snapshot().Generation; LastSequence=Snapshot().Sequence; LastSourceTime=Snapshot().SourceSeconds;
    Batch=FGuid::NewGuid().ToString(); StepIndex=0; MixingSeconds=0; MixingStarted=false; AwaitMix=false; MustReset=false; DueMissed=-1; StartedCharges.Empty();
    ExceptionReason.Empty();Run=ERun::Running; Message=TEXT("자동 실행 중"); EnterStep(); return true;
}
int32 FController::NextCharge() const
{
    for(int32 I=StepIndex;I<Recipe.Steps.Num();++I) if(Recipe.Steps[I].Kind==EStepKind::MixerCharge) return I;
    return INDEX_NONE;
}
void FController::EnterStep()
{
    if(StepIndex>=Recipe.Steps.Num()) { Operation=EOperation::Mixing; return; }
    const FStep& S=Recipe.Steps[StepIndex];
    if(S.Kind==EStepKind::WeighTransfer)
    {
        int32 Charge=NextCharge(); ReceiverOwner=Charge!=INDEX_NONE?Recipe.Steps[Charge].Id:TEXT("");
        Operation=EOperation::Feed; Send(ECommandKind::Feed,S.HopperId,S.TargetKg,S.Id);
    }
    else
    {
        Operation=EOperation::WaitCharge;
        if(StartedCharges.Contains(S.Id))
        {
            double Target=0;for(int32 I=StepIndex-1;I>=0&&Recipe.Steps[I].Kind==EStepKind::WeighTransfer;--I)Target+=Recipe.Steps[I].TargetKg;
            Operation=EOperation::Charge;Send(ECommandKind::Charge,TEXT("receiver"),Target,S.Id);
        }
    }
}
void FController::Tick(double Seconds)
{
    double ProviderSeconds=Seconds;
    const int32 Upcoming=NextCharge();
    if(Run==ERun::Running&&MixingStarted&&!AwaitMix&&Upcoming!=INDEX_NONE&&!StartedCharges.Contains(Recipe.Steps[Upcoming].Id)&&Recipe.Steps[Upcoming].Timing==ETiming::AfterMixing)
        ProviderSeconds=FMath::Min(Seconds,FMath::Max(0.0,Recipe.Steps[Upcoming].AtSeconds-MixingSeconds));
    Provider->Tick(ProviderSeconds);
    const auto& S=Snapshot();
    if(S.Sequence==ObservedSequence)NoUpdateSeconds+=Seconds;else{ObservedSequence=S.Sequence;NoUpdateSeconds=0;}
    if(ResetPending)
    {
        bool Empty=S.Weighed.IsEmpty()&&S.Transferred.IsEmpty()&&S.Charged.IsEmpty();
        bool ResetSafe=NoUpdateSeconds<=2&&!S.Devices.IsEmpty();for(const auto& D:S.Devices)if(!D.Stopped.Valid()||D.Stopped.Value!=1||(!D.ValveId.IsEmpty()&&(!D.ValveClosed.Valid()||D.ValveClosed.Value!=1)))ResetSafe=false;
        for(const auto& D:S.Devices)if((D.Id==TEXT("weigher")||D.Id==TEXT("receiver")||D.Id==TEXT("mixer"))&&(!D.Mass.Valid()||D.Mass.Value!=0))Empty=false;
        if(S.Generation!=Generation && ResetSafe && S.LedgerValid && !S.WrongMaterial && Empty)
        { ResetPending=false;LastStopRequest=0; Generation=S.Generation; Run=ERun::Idle; Operation=EOperation::None; StepIndex=0; MixingSeconds=0; MixingStarted=false; AwaitMix=false; MustReset=false; DueMissed=-1; ReceiverOwner.Empty();Batch.Empty();StartedCharges.Empty();ExceptionReason.Empty();Message=TEXT("초기화 실제 상태 확인 완료. 자동 실행하지 않습니다."); }
        return;
    }
    if(S.WrongMaterial && Run==ERun::Paused) { MustReset=true;ExceptionReason=Message=TEXT("잘못된 원료 유입 확인: 배치 중단·초기화 필요"); }
    if(Run!=ERun::Running) return;
    if(S.Generation!=Generation) { MustReset=true; Pause(TEXT("공급원 초기화 세대 변경: 배치 상태 재구성 불가")); return; }
    if(S.WrongMaterial) { MustReset=true; Pause(TEXT("잘못된 원료 유입 확인: 배치 중단·초기화 필요")); return; }
    if(!S.Error.IsEmpty()) { Pause(S.Error); return; }
    for(const auto& R:S.Receipts) if(R.Generation==Generation && R.Request==ActiveRequest && (R.Result==ECommandResult::Failed||R.Result==ECommandResult::Rejected)) { Pause(TEXT("명령 실패: ")+R.Device+TEXT(" ")+R.Detail); return; }
    if(S.Sequence==LastSequence) { if(NoUpdateSeconds>2) Pause(TEXT("상태 갱신 2초 이상 없음: 원인 미확인, 연결 확인 필요")); return; }
    LastSequence=S.Sequence;
    double Dt=S.SourceSeconds-LastSourceTime; LastSourceTime=S.SourceSeconds;
    if(Dt<0||!FMath::IsFinite(Dt)||!S.LedgerValid) { Pause(TEXT("공급원 시간 또는 누적량 유효성 확인 실패")); return; }
    if(Operation==EOperation::Stopping)
    { if(ConfirmedSafe()) { Run=ERun::Complete; Message=TEXT("배치 완료 · 실제 정지/닫힘 확인됨"); } return; }
    if(AwaitMix)
    {
        if(CommandApplied() && S.ActualRPM.Valid() && S.ActualRPM.Value>0) { AwaitMix=false; MixingStarted=true; EnterStep(); }
        return;
    }
    if(MixingStarted)
    {
        if(!S.ActualRPM.Valid()||S.ActualRPM.Value<=0) { Pause(TEXT("실제 혼합 가동 확인 없음")); return; }
        const int32 Charge=NextCharge();
        if(Charge!=INDEX_NONE && Recipe.Steps[Charge].Timing==ETiming::AfterMixing && !StartedCharges.Contains(Recipe.Steps[Charge].Id))
        {
            const double Due=Recipe.Steps[Charge].AtSeconds;
            if(MixingSeconds+Dt>=Due)
            {
                // Clamp the process clock at the deadline. No automatic late dispatch.
                MixingSeconds=Due;
                if(StepIndex!=Charge||Operation!=EOperation::WaitCharge)
                { DueMissed=Due; MustReset=true; Pause(TEXT("예약 시점 도달, 준비 미완료. 지연 원인은 미확인입니다. 예약 변경·재개 불가: 배치 중단 후 초기화 필요.")); return; }
            }
            else MixingSeconds+=Dt;
        }
        else MixingSeconds+=Dt;
    }
    if(StepIndex<Recipe.Steps.Num())
    {
        const auto& Step=Recipe.Steps[StepIndex];
        switch(Operation)
        {
        case EOperation::Feed:
            if(CommandApplied()&&ConditionsCurrent()&&S.SupplyEnded&&Amount(S.Weighed,Step.Id)+1e-8>=Step.TargetKg) Operation=EOperation::Settle;
            break;
        case EOperation::Settle:
        {
            bool Closed=false; for(const auto& D:S.Devices) if(D.Id==Step.HopperId) Closed=D.ValveClosed.Valid()&&D.ValveClosed.Value==1;
            if(Closed&&ConditionsCurrent()&&S.Stable) { Operation=EOperation::Transfer; Send(ECommandKind::Transfer,TEXT("weigher"),Step.TargetKg,Step.Id); }
            break;
        }
        case EOperation::Transfer:
            if(CommandApplied()&&ConditionsCurrent()&&S.DischargeComplete&&S.PipeClear&&Amount(S.Transferred,Step.Id)+1e-8>=Step.TargetKg) { ++StepIndex; EnterStep(); }
            break;
        case EOperation::WaitCharge:
            if(Step.Timing==ETiming::Initial||MixingSeconds>=Step.AtSeconds)
            {
                double Target=0; for(int32 I=StepIndex-1;I>=0&&Recipe.Steps[I].Kind!=EStepKind::MixerCharge;--I) Target+=Recipe.Steps[I].TargetKg;
                Operation=EOperation::Charge; StartedCharges.Add(Step.Id); Send(ECommandKind::Charge,TEXT("receiver"),Target,Step.Id);
            }
            break;
        case EOperation::Charge:
            if(CommandApplied()&&ConditionsCurrent()&&S.DischargeComplete&&S.PipeClear)
            {
                ReceiverOwner.Empty(); ++StepIndex;
                if(!MixingStarted) { AwaitMix=true; Send(ECommandKind::Mix,TEXT("mixer"),Recipe.RPM); }
                else EnterStep();
            }
            break;
        default: break;
        }
    }
    else if(MixingSeconds>=Recipe.MixSeconds && Operation!=EOperation::Stopping)
    { Operation=EOperation::Stopping; Send(ECommandKind::StopAll); }
    if(Operation==EOperation::Stopping && ConfirmedSafe()) { Run=ERun::Complete; Message=TEXT("배치 완료 · 실제 정지/닫힘 확인됨"); }
}
bool FController::ConfirmedSafe() const
{
    if(Snapshot().Devices.IsEmpty()||NoUpdateSeconds>2) return false;
    if(LastStopRequest!=0&&(Snapshot().StopConfirmedRequest!=LastStopRequest||Snapshot().Generation!=LastStopGeneration))return false;
    for(const auto& D:Snapshot().Devices)
        if(!D.Stopped.Valid()||D.Stopped.Value!=1||(!D.ValveId.IsEmpty()&&(!D.ValveClosed.Valid()||D.ValveClosed.Value!=1))) return false;
    return true;
}
void FController::CaptureLedger() { PauseWeighed=Snapshot().Weighed; PauseTransferred=Snapshot().Transferred; PauseCharged=Snapshot().Charged; }
bool FController::LedgerConsistent() const
{
    if(!Snapshot().LedgerValid||Snapshot().WrongMaterial||Snapshot().Generation!=Generation) return false;
    auto Valid=[&](const TMap<FString,double>& Before,const TMap<FString,double>& After,EStepKind Kind)
    {
        for(const auto& X:Before) if(Amount(After,X.Key)+1e-8<X.Value) return false;
        for(const auto& X:After)
        {
            const FStep* St=Recipe.Steps.FindByPredicate([&](const FStep& Y){return Y.Id==X.Key&&Y.Kind==Kind;});
            if(!St||!FMath::IsFinite(X.Value)||X.Value<0) return false;
            if(int32(St-Recipe.Steps.GetData())>StepIndex&&X.Value>0)return false;
            double Limit=St->TargetKg;
            if(Kind==EStepKind::MixerCharge) { Limit=0; int32 Index=int32(St-Recipe.Steps.GetData()); for(int32 I=Index-1;I>=0&&Recipe.Steps[I].Kind==EStepKind::WeighTransfer;--I) Limit+=Recipe.Steps[I].TargetKg; }
            if(X.Value>Limit+1e-8) return false;
        }
        return true;
    };
    if(!Valid(PauseWeighed,Snapshot().Weighed,EStepKind::WeighTransfer)||!Valid(PauseTransferred,Snapshot().Transferred,EStepKind::WeighTransfer)||!Valid(PauseCharged,Snapshot().Charged,EStepKind::MixerCharge)) return false;
    double W=0,T=0,C=0;
    for(const auto& X:Snapshot().Weighed) W+=X.Value;
    for(const auto& X:Snapshot().Transferred) { if(X.Value>Amount(Snapshot().Weighed,X.Key)+1e-8)return false; T+=X.Value; }
    for(const auto& X:Snapshot().Charged) C+=X.Value;
    for(const auto& D:Snapshot().Devices)
    {
        double Expected=-1;
        if(D.Id==TEXT("weigher"))Expected=W-T;
        if(D.Id==TEXT("receiver"))Expected=T-C;
        if(D.Id==TEXT("mixer"))Expected=C;
        if(Expected>=0&&(!D.Mass.Valid()||!FMath::IsNearlyEqual(D.Mass.Value,Expected,1e-7)))return false;
    }
    return true;
}
void FController::Pause(const FString& Reason)
{
    if(Run==ERun::Aborted) return;
    if(Run!=ERun::Paused) { SuspendedOperation=Operation; CaptureLedger();ExceptionReason=Reason; }
    Run=ERun::Paused; Message=Reason+TEXT("\n자동 순서·혼합 시계 중단. 정지/닫힘 실제 확인 후 사용자 조치 필요.");
    Send(ECommandKind::StopAll);
}
bool FController::Resume()
{
    if(Batch.IsEmpty()) { Message=TEXT("실행 중이던 배치가 없습니다. 수동 개입 후 초기화하세요.");return false; }
    if(Run!=ERun::Paused||MustReset) { Message=TEXT("재개 불가: 배치 중단·초기화 필요"); return false; }
    if(!Snapshot().Error.IsEmpty()) {Message=TEXT("재개 불가: 공급원 오류가 해결되지 않았습니다.");return false;}
    if(!ConfirmedSafe()||!LedgerConsistent()) { Message=TEXT("재개 불가: 실제 정지·닫힘 또는 누적량/원료 일치 미확인"); return false; }
    if(DueMissed>=0) { Message=TEXT("예약 준비 미완료: 재개 불가, 배치 중단 후 초기화 필요"); return false; }
    Run=ERun::Running; Operation=SuspendedOperation; LastSourceTime=Snapshot().SourceSeconds; LastSequence=Snapshot().Sequence;
    Message=TEXT("명시적 재개 · 확인된 누적량을 보존하고 남은 양만 처리");
    ExceptionReason.Empty();
    if(MixingStarted||AwaitMix) { AwaitMix=true; Send(ECommandKind::Mix,TEXT("mixer"),Recipe.RPM); }
    else EnterStep();
    // EnterStep reissues an absolute cumulative target; provider must not interpret it as an extra dose.
    return true;
}
void FController::Abort() { Send(ECommandKind::StopAll); Run=ERun::Aborted; MustReset=true; Message=TEXT("배치 중단 · 실제 정지/닫힘 확인 후 초기화 필요"); }
bool FController::Reset()
{
    if(DueMissed>=0&&Run!=ERun::Aborted) { Message=TEXT("예약 준비 미완료: 먼저 배치를 중단한 뒤 초기화하세요."); return false; }
    if(Run==ERun::Running||!ConfirmedSafe()) { Message=TEXT("초기화 불가: 먼저 정지하고 실제 정지·닫힘 확인 필요"); return false; }
    Generation=Snapshot().Generation; Send(ECommandKind::Reset); ResetPending=true; Message=TEXT("초기화 요청됨 · 새 세대/빈 장비 확인 대기"); return true;
}
void FController::ManualValve(const FString& Id,bool Open)
{
    if(Run==ERun::Running||Run==ERun::Idle) Pause(TEXT("사용자 수동 개입"));
    if(Open&&!ConfirmedSafe()){Message=TEXT("전체 정지 요청됨. 실제 정지·닫힘 확인 후 수동 열기를 다시 요청하세요.");return;}
    Send(Open?ECommandKind::OpenValve:ECommandKind::CloseValve,Id);
    Message=Open?TEXT("수동 열기 요청 · 자동 진행 중단 유지 · 재개 전 닫힘 확인 필요"):TEXT("닫기 요청 · 전체 정지 확인과 독립적으로 처리 · 실제 닫힘 확인 대기");
}
FString FController::StatusText() const
{
    switch(Run) { case ERun::Idle:return TEXT("편집 / 대기"); case ERun::Running:return TEXT("자동 운전"); case ERun::Paused:return TEXT("사용자 조치 대기"); case ERun::Aborted:return TEXT("배치 중단"); case ERun::Complete:return TEXT("완료"); } return TEXT("");
}
FString FController::WaitingText() const
{
    switch(Operation) {case EOperation::Feed:return TEXT("공급 종료 및 목표 누적량 확인");case EOperation::Settle:return TEXT("공급 밸브 닫힘·계량 안정 확인");case EOperation::Transfer:return TEXT("배출·배관 이송 완료 확인");case EOperation::WaitCharge:return TEXT("믹서 투입 조건 대기");case EOperation::Charge:return TEXT("수취호퍼 배출·배관 이송 완료 확인");case EOperation::Mixing:return TEXT("혼합 시간 완료 대기");case EOperation::Stopping:return TEXT("실제 정지·밸브 닫힘 확인");default:return TEXT("실행 전 검증");}
}
}
