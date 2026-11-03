#include "ProcessTestProvider.h"
namespace ProcessUI
{
FTestProvider::FTestProvider(const TArray<FHopper>& Hoppers)
{
    State.Source=TEXT("교체형 테스트 공급원 / 물리 해석 아님"); State.Generation=1; State.TestMode=true; State.CanAutomate=true; State.LedgerValid=true;
    State.Capabilities={TEXT("cumulative_mass"),TEXT("material_inventory"),TEXT("valve_feedback"),TEXT("stop_feedback"),TEXT("sequence_conditions"),TEXT("timed_charge"),TEXT("reset_generation")};
    auto Add=[&](const FString& Id,const FString& Label,bool Valve)
    {
        FDevice D; D.Id=Id; D.Label=Label; D.ValveId=Valve?Id+TEXT(".outlet"):TEXT("");
        D.Mass=FReading::Known(0,TEXT("kg"),0); D.TotalIn=D.TotalOut=D.Mass;
        D.Stopped=FReading::Known(1,TEXT("bool"),0); if(Valve) D.ValveClosed=D.Stopped;
        D.MaterialsValid=true; State.Devices.Add(D);
    };
    for(const auto& H:Hoppers) { Add(H.Id,H.Id+TEXT(" / ")+H.Material,true); State.Devices.Last().Mass.Validity=EValidity::Missing;State.Devices.Last().MaterialsValid=false; }
    Add(TEXT("weigher"),TEXT("공통 칭량호퍼"),true); Add(TEXT("receiver"),TEXT("수취호퍼"),true); Add(TEXT("mixer"),TEXT("혼합기"),false);
    State.ActualRPM=State.AppliedRPM=FReading::Known(0,TEXT("RPM"),0);
}
FDevice* FTestProvider::Device(const FString& Id) { return State.Devices.FindByPredicate([&](const FDevice& D){return D.Id==Id||D.ValveId==Id;}); }
void FTestProvider::SetReading(FReading& R,double V,const TCHAR* U) { R=FReading::Known(V,U,State.SourceSeconds); }
void FTestProvider::Receipt(const FCommand& C,ECommandResult Result,const FString& Detail)
{
    FReceipt R{C.Request,C.Generation,Result,C.Device,Detail}; Seen.Add(C.Request,R);
    R.Step=C.Step;
    if(Result==ECommandResult::Applied && (C.Kind==ECommandKind::Feed||C.Kind==ECommandKind::Transfer||C.Kind==ECommandKind::Charge))R.AppliedTarget=FReading::Known(C.Target,TEXT("kg"),State.SourceSeconds);
    State.Receipts.RemoveAll([&](const FReceipt& X){return X.Request==C.Request;}); State.Receipts.Add(R);
    if(State.Receipts.Num()>128) State.Receipts.RemoveAt(0);
}
void FTestProvider::Submit(const FCommand& C)
{
    if(C.Generation!=State.Generation) { Receipt(C,ECommandResult::Rejected,TEXT("초기화 세대 불일치")); return; }
    if(Seen.Contains(C.Request)) return;
    if(C.Kind==ECommandKind::Reset)
    {
        for(const auto& D:State.Devices) if(D.Stopped.Value!=1||(!D.ValveId.IsEmpty()&&D.ValveClosed.Value!=1)) { Receipt(C,ECommandResult::Rejected,TEXT("정지·닫힘 미확인")); return; }
        for(auto& D:State.Devices) { if(D.Mass.Valid()) SetReading(D.Mass,0,TEXT("kg")); SetReading(D.TotalIn,0,TEXT("kg")); SetReading(D.TotalOut,0,TEXT("kg")); D.MaterialKg.Empty();D.AppliedRate={};D.ActualRate={};D.AppliedDiameter={};D.Error.Empty(); }
        State.Weighed.Empty(); State.Transferred.Empty(); State.Charged.Empty();State.ChargeStartedAt.Empty(); State.WrongMaterial=false; State.Error.Empty(); ++State.Generation;State.ConditionsRequest=0;State.ConditionsValidity=EValidity::Missing;
        State.Receipts.Empty(); Seen.Empty(); HasActive=false; Stopping=false; RequestedRPM=0; SetReading(State.ActualRPM,0,TEXT("RPM")); SetReading(State.AppliedRPM,0,TEXT("RPM"));
        return;
    }
    if(C.Kind==ECommandKind::StopAll)
    { StopCommand=C;Stopping=true; StopDelay=0.3; RequestedRPM=0; Receipt(C,ECommandResult::Applied,TEXT("정지/닫힘 요청 적용 · 실제 확인 대기")); return; }
    if(C.Kind==ECommandKind::OpenValve||C.Kind==ECommandKind::CloseValve)
    {
        FDevice* D=Device(C.Device);
        if(!D||D->ValveId.IsEmpty()) { Receipt(C,ECommandResult::Rejected,TEXT("밸브 ID 미지원")); return; }
        if(C.Kind==ECommandKind::CloseValve&&FailCloseValve==D->ValveId) { D->Error=TEXT("테스트: 닫힘 실패"); Receipt(C,ECommandResult::Failed,D->Error); return; }
        SetReading(D->ValveClosed,C.Kind==ECommandKind::CloseValve?1:0,TEXT("bool"));
        if(C.Kind==ECommandKind::CloseValve){D->Error.Empty();if(HasActive&&D->Id==Active.Device){HasActive=false;State.ConditionsValidity=EValidity::Missing;SetReading(D->ActualRate,0,TEXT("kg/s"));}}
        SetReading(D->Stopped,C.Kind==ECommandKind::CloseValve?1:0,TEXT("bool")); Receipt(C,ECommandResult::Applied,TEXT("테스트 밸브 상태 적용")); return;
    }
    if(Stopping) { Receipt(C,ECommandResult::Rejected,TEXT("정지 처리 중")); return; }
    Active=C; HasActive=true; Delay=0.1; FinishWait=-1;
    if(C.Kind!=ECommandKind::Mix) { State.SupplyEnded=false; State.Stable=false; State.DischargeComplete=false; State.PipeClear=false;State.ConditionsRequest=C.Request;State.ConditionsValidity=EValidity::Valid; }
    // This test provider supports immediate charge start at the scheduler boundary.
    // Real adapters must advertise/implement timed_charge, not infer it from request acceptance.
    if(C.Kind==ECommandKind::Charge){Delay=0;if(!State.ChargeStartedAt.Contains(C.Step))State.ChargeStartedAt.Add(C.Step,State.SourceSeconds);Receipt(C,ECommandResult::Applied,TEXT("테스트 믹서 투입 시작 적용"));return;}
    Receipt(C,ECommandResult::Accepted,TEXT("접수됨 · 적용 대기"));
}
void FTestProvider::Tick(double Seconds)
{
    const double Dt=FMath::Clamp(Seconds,0.0,0.1); State.SourceSeconds+=Dt; State.ReceivedSeconds=State.SourceSeconds; ++State.Sequence;
    if(Stopping)
    {
        StopDelay-=Dt;
        if(StopDelay<=0)
        {
            Stopping=false; HasActive=false;State.StopConfirmedRequest=StopCommand.Request; SetReading(State.ActualRPM,0,TEXT("RPM")); SetReading(State.AppliedRPM,0,TEXT("RPM"));
            for(auto& D:State.Devices)
            {
                SetReading(D.Stopped,1,TEXT("bool")); if(D.ActualRate.Valid())SetReading(D.ActualRate,0,TEXT("kg/s"));
                if(!D.ValveId.IsEmpty())
                { if(D.ValveId==FailCloseValve) { D.Error=TEXT("테스트: 닫힘 확인 실패"); SetReading(D.ValveClosed,0,TEXT("bool")); }
                  else { SetReading(D.ValveClosed,1,TEXT("bool")); D.Error.Empty(); } }
            }
        }
        // Material already in motion may keep moving during the acknowledgement interval.
    }
    if(!HasActive) return;
    Delay-=Dt; if(Delay>0) return;
    Receipt(Active,ECommandResult::Applied,TEXT("적용됨 · 실제 공정 완료는 상태로 확인"));
    if(Active.Kind==ECommandKind::Mix)
    { RequestedRPM=Active.Target; SetReading(State.AppliedRPM,RequestedRPM,TEXT("RPM")); SetReading(State.ActualRPM,RequestedRPM,TEXT("RPM")); SetReading(Device(TEXT("mixer"))->Stopped,0,TEXT("bool")); HasActive=false; return; }
    FDevice* From=nullptr; FDevice* To=nullptr; double* Progress=nullptr; double Rate=Bounds().TransferKgS;
    if(Active.Kind==ECommandKind::Feed)
    { From=Device(Active.Device); To=Device(TEXT("weigher")); Progress=&State.Weighed.FindOrAdd(Active.Step); Rate=Active.Rate*PreparationSpeed; StepHopper.Add(Active.Step,Active.Device); }
    else if(Active.Kind==ECommandKind::Transfer)
    { From=Device(TEXT("weigher")); To=Device(TEXT("receiver")); Progress=&State.Transferred.FindOrAdd(Active.Step); Rate*=PreparationSpeed; }
    else if(Active.Kind==ECommandKind::Charge)
    { From=Device(TEXT("receiver")); To=Device(TEXT("mixer")); Progress=&State.Charged.FindOrAdd(Active.Step); }
    if(!From||!To||!Progress) { Receipt(Active,ECommandResult::Failed,TEXT("알 수 없는 명령 대상")); HasActive=false; return; }
    if(Active.Kind==ECommandKind::Feed) {SetReading(From->AppliedRate,Active.Rate,TEXT("kg/s"));SetReading(From->AppliedDiameter,Active.DiameterM,TEXT("m"));}
    double Move=FMath::Min(FMath::Max(0.0,Active.Target-*Progress),Rate*Dt);
    if(From->Mass.Valid()) Move=FMath::Min(Move,From->Mass.Value);
    SetReading(From->ActualRate,Dt>0?Move/Dt:0,TEXT("kg/s"));
    if(Active.Kind==ECommandKind::Feed)To->MaterialKg.FindOrAdd(Active.Material)+=Move;
    else if(From->Mass.Value>0)
        for(auto& X:From->MaterialKg){const double Part=Move*X.Value/From->Mass.Value;X.Value-=Part;To->MaterialKg.FindOrAdd(X.Key)+=Part;}
    SetReading(From->Stopped,0,TEXT("bool")); SetReading(From->ValveClosed,0,TEXT("bool"));
    *Progress+=Move; if(From->Mass.Valid()) SetReading(From->Mass,From->Mass.Value-Move,TEXT("kg"));
    SetReading(To->Mass,To->Mass.Value+Move,TEXT("kg")); SetReading(From->TotalOut,From->TotalOut.Value+Move,TEXT("kg")); SetReading(To->TotalIn,To->TotalIn.Value+Move,TEXT("kg"));
    if(*Progress+1e-8>=Active.Target)
    {
        SetReading(From->ValveClosed,1,TEXT("bool")); SetReading(From->Stopped,1,TEXT("bool"));
        State.SupplyEnded=true;
        // A separate settling/pipe-clear interval is part of this provider's declared test bounds.
        if(FinishWait<0)FinishWait=Bounds().SettleSeconds;
        FinishWait-=Dt;
        if(FinishWait>0) return;
        State.Stable=true; State.DischargeComplete=true; State.PipeClear=true; HasActive=false;
    }
}
}
