#include "PlantSupplyABatch.h"
#include "PlantDoseValidation.h"

FPlantSupplyABatch::FPlantSupplyABatch(const powder::GranularConfig& C,const TArray<int32>& SourceLabels,
    const FPlantTransferTelemetry& M,const FPlantFeedPreparation& P,bool Enabled):Labels(SourceLabels),Geometry(C,Enabled),Mass(M),Preparation(P)
{
    ForeignSeen.Init(0,Labels.Num());
    Recovery.Supported=Enabled;
    State.Supported=Feed.Supported=Enabled;if(!Enabled)return;
    FPlantSimulationSnapshot Initial;Mass.Publish(Initial);Criteria=Initial.Transfer.Criteria;
    if(!Geometry.Error.IsEmpty()){State.Error=Geometry.Error;return;}
    if(Labels.Num()!=int32(C.count)||Mass.ParticleMassKg<=0){State.Error=TEXT("초기 입자 ID·공급 장비·질량 정보를 확인할 수 없습니다.");return;}
    const auto Gates=powder::plant_supplier_gates(C.plant_data);
    if(Gates.size()!=6){State.Error=TEXT("A 배치의 공급 게이트를 확인할 수 없습니다.");return;}
    for(int32 Label:Labels)if(Label<0||Label>=6){State.Error=TEXT("출처 불명 입자 · 장입 파일의 공급원 ID를 확인하세요.");return;}
    Sequential.Materials.SetNum(6);
    for(int32 Label:Labels)if(Sequential.Materials.IsValidIndex(Label))++Sequential.Materials[Label].InitialCount;
    LayoutValid=true;SelectSource(0);
}
void FPlantSupplyABatch::SelectSource(int32 Supplier)
{
    Sequential.ActiveSupplier=Supplier;

    IsA.Reset(Labels.Num());
    for(int32 Label:Labels)IsA.Add(Label==Supplier);
    SelectedDose.Reset();
    State=FPlantABatchSnapshot();
    State.Supported=true;
    State.InitialCount=State.TotalCount=Sequential.Materials[Supplier].InitialCount;
    State.InitialKg=State.TotalKg=State.TotalCount*Mass.ForSource(Sequential.ActiveSupplier);
}
void FPlantSupplyABatch::Observe(const std::vector<powder::Grain>& P,double Time)
{
    if(!State.Supported||!LayoutValid)return;
    State.Valid=false;Feed.Valid=false;
    if(!State.Error.IsEmpty())return;
    if(P.size()!=size_t(IsA.Num())||!FMath::IsFinite(Time)||Time<State.ObservedSimulationSeconds)
    {State.Error=TEXT("A 배치의 입자 ID 또는 해석 시간 연속성이 끊겼습니다.");return;}
    State.ObservedSimulationSeconds=Time;
    Recovery.Valid=false;Recovery.ObservedSeconds=Time;
    if(Sequential.Materials.Num()!=6)return;
    Recovery.Materials.SetNum(6);
    for(int32 I=0;I<6;++I){Recovery.Materials[I]=FPlantRecoveryMaterial();Recovery.Materials[I].Supplier=I;}

    for(auto& Entry:Sequential.Materials){auto* M=&Entry;M->SourceCount=M->SelectedCount=M->ReceiverCount=M->MixerCount=M->ConnectorCount=0;M->CohortHash=1469598103934665603LL;}
    State.SourceCount=State.ConnectorCount=State.WeighingCount=State.PipeCount=State.ReceiverCount=State.MixerConnectorCount=State.MixerCount=State.UnknownCount=State.ForeignCount=0;
    State.InventorySourceCount=0;TArray<int32> CurrentWeighing;
    State.ObservedCohortKg=0;Feed.ReceiverKg=Feed.ConnectorKg=Feed.MixerKg=0;
    for(int32 I=0;I<IsA.Num();++I)
    {
        const auto Region=Mass.Classify(P[I]);
        if(Region==EPlantMassRegion::Invalid){State.Error=TEXT("A 배치 관찰 중 유효하지 않은 입자 좌표가 있습니다.");return;}
        const int32 Path=Preparation.SupplyPath(P[I]);
        const auto Downstream=Geometry.Classify(P[I]);
        // Reuse this exact classification for both current-cohort and per-material accounting.
        // Do not scan the full route geometry again for each ledger.
        const int32 Label=Labels[I];
        const bool AtSource=Path==INDEX_NONE&&Region==EPlantMassRegion::Outside&&Downstream==EPlantMixerRegion::Outside&&Preparation.InSupplier(Label,P[I]);
        if(Sequential.Materials.IsValidIndex(Label)){
            auto& R=Recovery.Materials[Label];
            const auto& Cohort=Cohorts[Label];
            if(Cohort.IsValidIndex(I)&&Cohort[I])R.SelectedIds.Add(I);
            if(Region==EPlantMassRegion::Weighing)++R.Weighing;
            else if(Region==EPlantMassRegion::Pipe)++R.Pipe;
            else if(Region==EPlantMassRegion::Receiver)++R.Receiver;
            else if(Downstream==EPlantMixerRegion::Mixer)++R.Mixer;
            else if(Downstream==EPlantMixerRegion::Connector)++R.MixerConnector;
            else if(Path==Label)++R.SupplyConnector;
            else if(AtSource)++R.Source;
            else ++R.Unknown;
            if(Region==EPlantMassRegion::Weighing||Region==EPlantMassRegion::Pipe||Region==EPlantMassRegion::Receiver||Downstream!=EPlantMixerRegion::Outside||Path==Label)R.PassedIds.Add(I);
            if(Region==EPlantMassRegion::Receiver)R.ReceiverIds.Add(I);
            if(Downstream==EPlantMixerRegion::Mixer)R.MixerIds.Add(I);

            auto& M=Sequential.Materials[Label];const auto& IDs=Cohorts[Label];
            if(AtSource)++M.SourceCount;
            if(IDs.IsValidIndex(I)&&IDs[I]){
                ++M.SelectedCount;M.CohortHash=int64(((uint64(M.CohortHash)^uint64(I))*1099511628211ULL)&0x7fffffffffffffffULL);
                if(Region==EPlantMassRegion::Receiver)++M.ReceiverCount;
                if(Downstream==EPlantMixerRegion::Mixer)++M.MixerCount;
                if(Downstream==EPlantMixerRegion::Connector)++M.ConnectorCount;
            }
        }
        if(IsA[I]&&Region==EPlantMassRegion::Weighing)CurrentWeighing.Add(I);
        if(IsA[I]&&AtSource)++State.InventorySourceCount;
        if(!Sequential.Combined&&Label!=Sequential.ActiveSupplier&&Sequential.Materials.IsValidIndex(Label)&&Cohorts[Label].IsValidIndex(I)&&Cohorts[Label][I]){
            if(Region!=EPlantMassRegion::Receiver){++State.ForeignCount;Foreign(I,TEXT("선행 원료 수취호퍼 이탈"));}
            continue; // A is held, never included in B arrival or B final dose.
        }
        if(!IsA[I]||(State.DoseFinalized&&!SelectedDose[I])){
            const bool WasMixer=ExistingMixer.IsValidIndex(I)&&ExistingMixer[I];
            const bool Process=Path!=INDEX_NONE||Region==EPlantMassRegion::Weighing||Region==EPlantMassRegion::Pipe||Region==EPlantMassRegion::Receiver||Downstream==EPlantMixerRegion::Connector;
            const bool NewMixer=Downstream==EPlantMixerRegion::Mixer&&!WasMixer&&State.CaptureRevision>0;
            const bool EscapedMixer=WasMixer&&Downstream!=EPlantMixerRegion::Mixer;
            if(Process||NewMixer||EscapedMixer){++State.ForeignCount;
                Foreign(I,Path!=INDEX_NONE?FString::Printf(TEXT("공급 연결부 %d"),Path):Region==EPlantMassRegion::Weighing?TEXT("칭량호퍼"):Region==EPlantMassRegion::Pipe?TEXT("이송 배관"):Region==EPlantMassRegion::Receiver?TEXT("수취호퍼"):Downstream==EPlantMixerRegion::Connector?TEXT("혼합 연결부"):TEXT("혼합기/보관 이탈"));
                if(Feed.OpeningRevision>0&&UnexpectedSeen.IsValidIndex(I)&&!UnexpectedSeen[I]){UnexpectedSeen[I]=1;++Feed.AdditionalInflowCount;}}
            continue;
        }
        State.ObservedCohortKg+=Mass.ForId(I);
        // Upstream membership is bounded by the selected hopper route above its gate.
        if(Region==EPlantMassRegion::Weighing)++State.WeighingCount;
        else if(Region==EPlantMassRegion::Pipe)++State.PipeCount;
        else if(Region==EPlantMassRegion::Receiver){++State.ReceiverCount;Feed.ReceiverKg+=Mass.ForId(I);}
        else if(Downstream==EPlantMixerRegion::Mixer){++State.MixerCount;Feed.MixerKg+=Mass.ForId(I);}
        else if(Downstream==EPlantMixerRegion::Connector){++State.MixerConnectorCount;Feed.ConnectorKg+=Mass.ForId(I);}
        else if(Path==Sequential.ActiveSupplier)++State.ConnectorCount;
        else if(AtSource)++State.SourceCount;
        else ++State.UnknownCount;
    }
    for(auto& Entry:Sequential.Materials){auto* M=&Entry;
        if(M->MixerCount>0&&M->FirstMixerSeconds<0)M->FirstMixerSeconds=Time;
        if(M->SelectedCount>0&&M->MixerCount==M->SelectedCount&&M->AllMixerSeconds<0)M->AllMixerSeconds=Time;
    }
    Sequential.A=Sequential.Materials[0];Sequential.B=Sequential.Materials[1];
    Sequential.Additive=Sequential.Materials[Sequential.AdditiveActive?Sequential.ActiveSupplier:2];
    // While an occupied gate is still open, publish the additional mass already through it.
    // Once actual closure is recorded this remains the immutable closure result.
    if(State.SupplyCloseRequestedRevision>0&&State.SupplyClosedRevision==0)
        State.ClosingExtraKg=(State.InitialCount-State.InventorySourceCount)*Mass.ForSource(Sequential.ActiveSupplier)-State.CloseRequestedPassedKg;
    if(CurrentWeighing!=WeighingIds||WeighingStableSince<0){WeighingIds=MoveTemp(CurrentWeighing);WeighingStableSince=Time;++State.WeighingSetRevision;}
    State.WeighingStableSeconds=Time-WeighingStableSince;
    State.Valid=State.Error.IsEmpty();Recovery.Valid=State.Valid&&State.UnknownCount==0;Feed.Valid=State.Valid;Feed.ObservedSimulationSeconds=Time;
    if(Feed.OpeningRevision<=0)return;
    if(Feed.MixerKg>0&&Feed.FirstArrivalSimulationSeconds<0)Feed.FirstArrivalSimulationSeconds=Time;
    if(State.ForeignCount||State.UnknownCount||State.SourceCount||State.ConnectorCount||State.WeighingCount||State.PipeCount){
        Feed.State=EPlantTransferState::Indeterminate;Feed.Reason=TEXT("배치 외 원료 유입·기존 혼합 원료 이동 또는 대상 입자 구역 이탈");}
    if(Feed.State==EPlantTransferState::Indeterminate){StableSince=-1;Feed.StableSeconds=0;return;}
    const bool Within=Feed.ReceiverKg<=Criteria.WeighingResidualKg&&Feed.ConnectorKg<=Criteria.PipeResidualKg&&
        FMath::Abs(Feed.MixerKg-Feed.BaselineKg)<=Criteria.MassErrorKg&&Feed.AdditionalInflowCount==0;
    if(!Within){StableSince=-1;Feed.StableSeconds=0;Feed.State=EPlantTransferState::Moving;Feed.Reason=TEXT("수취호퍼·혼합 연결부 잔류 확인 중");return;}
    if(StableSince<0)StableSince=Time;Feed.StableSeconds=Time-StableSince;
    Feed.State=Feed.StableSeconds>=Criteria.StableSimulationSeconds?EPlantTransferState::Complete:EPlantTransferState::Moving;
    Feed.Reason=Feed.State==EPlantTransferState::Complete?TEXT("이번 배치 전량 혼합기 도착·잔류0·안정 확인"):TEXT("혼합기 전량 도착 · 해석 시간으로 안정 관찰 중");
    if(Feed.State==EPlantTransferState::Complete&&Feed.CompleteSimulationSeconds<0)Feed.CompleteSimulationSeconds=Time;
}
bool FPlantSupplyABatch::Capture(const std::vector<powder::Grain>& P,double Time,int64 Revision,int32 Supplier)
{
    if(State.CaptureRevision==Revision&&Revision>0)return State.Valid&&Sequential.ActiveSupplier==Supplier;
    if(State.CaptureRevision||!State.Error.IsEmpty()||!Sequential.Materials.IsValidIndex(Supplier))return false;
    SelectSource(Supplier);
    Observe(P,Time);
    if(!State.Valid||Revision<=0||State.CaptureRevision||State.TotalCount<=0||State.SourceCount!=State.TotalCount||State.ForeignCount)
    {State.Error=TEXT("초기 A 원료 전량이 공급호퍼에 있고 공정이 비어 있는 새 배치만 시작할 수 있습니다. 초기화 후 확인하세요.");State.Valid=false;return false;}
    ExistingMixer.Init(0,IsA.Num());UnexpectedSeen.Init(0,IsA.Num());State.InitialMixerCount=0;
    for(int32 I=0;I<IsA.Num();++I)if(!IsA[I]&&Geometry.Classify(P[I])==EPlantMixerRegion::Mixer){ExistingMixer[I]=1;++State.InitialMixerCount;}
    Feed.InitialMixerKg=0;for(int32 I=0;I<ExistingMixer.Num();++I)if(ExistingMixer[I])Feed.InitialMixerKg+=Mass.ForId(I);
    State.CaptureRevision=Revision;State.CapturedSimulationSeconds=Time;return true;
}
bool FPlantSupplyABatch::VerifyOpening(const std::vector<powder::Grain>& P,double Time,int64 Revision)
{
    Observe(P,Time);
    return State.Valid&&Revision>0&&State.CaptureRevision==Revision&&State.TotalCount>0&&
        (State.WeighingCount==State.TotalCount||(RecoveryValid()&&State.DoseFinalized&&
         State.WeighingCount+State.PipeCount+State.ReceiverCount==State.TotalCount))&&State.ForeignCount==0&&State.UnknownCount==0;
}

void FPlantSupplyABatch::InvalidateCapture()
{
 if(!LayoutValid)return;
 Recovery=FPlantBatchRecoverySnapshot();Recovery.Supported=State.Supported;ForeignSeen.Init(0,Labels.Num());MixLastTime=-1;ReadyMixSeconds=-1;MixWasAutomatic=false;LastRecoveryAttempt=0;
 const bool Supported=State.Supported;State=FPlantABatchSnapshot();State.Supported=Supported;
 Sequential=FPlantSequentialBatchSnapshot();Sequential.Materials.SetNum(6);for(auto& IDs:Cohorts)IDs.Reset();
 InitialFeed=FPlantMixerFeedSnapshot();IsA.Reset();
 for(int32 Label:Labels){IsA.Add(Label==0);if(Label==0)++State.InitialCount;if(Sequential.Materials.IsValidIndex(Label))++Sequential.Materials[Label].InitialCount;}
 State.InitialKg=State.InitialCount*Mass.ForSource(Sequential.ActiveSupplier);
 State.CaptureRevision=0;State.CapturedSimulationSeconds=-1;State.InitialMixerCount=0;
 State.TotalCount=State.InitialCount;State.TotalKg=State.InitialKg;SelectedDose.Empty();WeighingIds.Empty();WeighingStableSince=-1;
 State.WeighingSetRevision=0;State.WeighingStableSeconds=0;State.DoseFinalized=false;
 State.DoseFinalizeRevision=State.DoseRejectedRevision=0;State.DoseReason.Empty();State.DoseFinalKg=State.DoseTargetKg=State.DoseToleranceKg=0;State.DoseFinalSimulationSeconds=-1;
 State.SupplyCloseRequestedRevision=State.SupplyClosedRevision=0;State.SupplyCloseRequestedSeconds=State.SupplyClosedSeconds=-1;
 State.CloseRequestedWeighingKg=State.CloseRequestedConnectorKg=State.CloseRequestedPassedKg=0;
 State.ClosedWeighingKg=State.ClosedConnectorKg=State.ClosedPassedKg=State.ClosingExtraKg=0;
 ExistingMixer.Empty();UnexpectedSeen.Empty();Feed=FPlantMixerFeedSnapshot();Feed.Supported=State.Supported;StableSince=-1;
}
void FPlantSupplyABatch::RejectDownstream(int64 Revision,const FString& Reason)
{
 Feed.RejectedRevision=Revision;Feed.State=EPlantTransferState::Indeterminate;Feed.Reason=Reason;Feed.StableSeconds=0;StableSince=-1;
}
bool FPlantSupplyABatch::BeginMixerFeed(const std::vector<powder::Grain>& P,double Time,int64 CaptureRevision,int64 Revision)
{
 Observe(P,Time);
 if(RecoveryValid()&&State.CaptureRevision==CaptureRevision&&Feed.OpeningRevision>0&&
    Feed.State!=EPlantTransferState::Indeterminate&&State.TotalCount>0&&State.ForeignCount==0&&State.UnknownCount==0&&
    State.ReceiverCount+State.MixerConnectorCount+State.MixerCount==State.TotalCount){
  Feed.OpeningRevision=Revision;return true;
 }
 if(!State.Valid||CaptureRevision<=0||State.CaptureRevision!=CaptureRevision||State.TotalCount<=0||
    State.ReceiverCount!=State.TotalCount||State.ForeignCount||State.UnknownCount||Feed.OpeningRevision>0){
  RejectDownstream(Revision,TEXT("수취 출구 개방 직전 A 배치 전량·ID·잔류 조건 불일치"));return false;}
 if(AdditiveScheduled()&&(!Sequential.AdditiveReady||InitialFeed.MixerAppliedRevision<=0||
    InitialFeed.MixerAppliedSimulationSeconds<0||Sequential.AdditiveReadySeconds<0||
    ReadyMixSeconds<0||ReadyMixSeconds>Sequential.AdditiveDelaySeconds+1e-12||
    Time<InitialFeed.MixerAppliedSimulationSeconds||Recovery.AutomaticMixSeconds+1e-12<Sequential.AdditiveDelaySeconds)){
  RejectDownstream(Revision,TEXT("예약 개방 금지 · 작업자 혼합 시작/준비/예약 해석 시각 미충족"));return false;
 }
 Feed.OpeningRevision=Revision;Feed.StartedSimulationSeconds=Time;Feed.BaselineKg=State.TotalKg;
 Feed.State=EPlantTransferState::Moving;Feed.Reason=TEXT("혼합기로 투입 중");StableSince=-1;return true;
}
void FPlantSupplyABatch::ReceiverClosed(double Time,int64 Revision)
{
 if(Feed.OpeningRevision>0){Feed.CloseRevision=Revision;Feed.ClosedSimulationSeconds=Time;}
}
bool FPlantSupplyABatch::AdmitMixerStart(const std::vector<powder::Grain>& P,double Time,int64 CaptureRevision,int64 Revision)
{
 Observe(P,Time);
 auto& F=AdditiveScheduled()?InitialFeed:Feed;
 const bool CohortReady=AdditiveScheduled()?
  ((Sequential.AdditiveReady||RecoveryValid())&&Sequential.RegularsInMixer()):
  State.MixerCount==State.TotalCount;
 if(!State.Valid||CaptureRevision<=0||State.CaptureRevision!=CaptureRevision||State.TotalCount<=0||
    !CohortReady||State.ForeignCount||State.UnknownCount||F.State!=EPlantTransferState::Complete||
    F.CloseRevision<=F.OpeningRevision||F.ClosedSimulationSeconds<0){
  RejectDownstream(Revision,TEXT("혼합 시작 직전 전량 도착·수취 출구 실제 닫힘/첨가제 준비 확인 불가"));return false;}
 F.MixerStartRevision=Revision;F.MixerStartSimulationSeconds=Time;return true;
}
void FPlantSupplyABatch::MixerApplied(double Time,int64 Revision,double RPM)
{
 auto& F=AdditiveScheduled()?InitialFeed:Feed;
 if(Revision==F.MixerStartRevision&&Revision>0&&RPM>0&&F.MixerAppliedRevision!=Revision){
  F.MixerAppliedRevision=Revision;F.MixerAppliedSimulationSeconds=Time;F.AppliedRPM=RPM;}
}

void FPlantSupplyABatch::SupplyCloseRequested(const std::vector<powder::Grain>& P,double Time,int64 Revision)
{
 if(State.CaptureRevision<=0||State.SupplyCloseRequestedRevision>0)return;
 Observe(P,Time);if(!State.Valid)return;
 State.SupplyCloseRequestedRevision=Revision;State.SupplyCloseRequestedSeconds=Time;
 State.CloseRequestedWeighingKg=State.WeighingCount*Mass.ForSource(Sequential.ActiveSupplier);
 State.CloseRequestedConnectorKg=State.ConnectorCount*Mass.ForSource(Sequential.ActiveSupplier);
 State.CloseRequestedPassedKg=(State.InitialCount-State.InventorySourceCount)*Mass.ForSource(Sequential.ActiveSupplier);
}
void FPlantSupplyABatch::SupplyClosed(const std::vector<powder::Grain>& P,double Time,int64 Revision)
{
 if(State.SupplyCloseRequestedRevision<=0||State.SupplyClosedRevision>0)return;
 Observe(P,Time);if(!State.Valid)return;
 State.SupplyClosedRevision=Revision;State.SupplyClosedSeconds=Time;
 State.ClosedWeighingKg=State.WeighingCount*Mass.ForSource(Sequential.ActiveSupplier);State.ClosedConnectorKg=State.ConnectorCount*Mass.ForSource(Sequential.ActiveSupplier);
 State.ClosedPassedKg=(State.InitialCount-State.InventorySourceCount)*Mass.ForSource(Sequential.ActiveSupplier);
 State.ClosingExtraKg=State.ClosedPassedKg-State.CloseRequestedPassedKg;
}
void FPlantSupplyABatch::RejectDose(int64 Revision,const FString& Reason)
{
 State.DoseRejectedRevision=Revision;State.DoseReason=Reason;
}
bool FPlantSupplyABatch::FinalizeDose(const std::vector<powder::Grain>& P,double Time,int64 CaptureRevision,int64 Revision,double TargetKg,double ToleranceKg)
{
 if(Revision>0&&State.DoseFinalizeRevision==Revision)return State.DoseFinalized;
 if(Revision>0&&State.DoseRejectedRevision==Revision)return false;
 Observe(P,Time);
 FString Why=PlantDose::Validate(TargetKg,ToleranceKg,Mass.ForSource(Sequential.ActiveSupplier),State.InitialKg);
 if(!State.Valid||CaptureRevision<=0||State.CaptureRevision!=CaptureRevision||Revision<=0||State.DoseFinalized||State.ForeignCount||State.UnknownCount||
    State.ConnectorCount||State.PipeCount||State.ReceiverCount||State.MixerCount||State.MixerConnectorCount||State.WeighingCount<=0||
    State.SourceCount+State.WeighingCount!=State.TotalCount||State.WeighingStableSeconds<Criteria.StableSimulationSeconds||!Why.IsEmpty()){
  RejectDose(Revision,TEXT("최종 계량의 배치·공급 잔류·동일 입자 안정 조건 미확인: ")+Why);return false;}
 State.DoseTargetKg=TargetKg;State.DoseToleranceKg=ToleranceKg;State.DoseFinalKg=State.WeighingCount*Mass.ForSource(Sequential.ActiveSupplier);State.DoseFinalSimulationSeconds=Time;
 if((ToleranceKg==0 && WeighingIds.Num()!=FMath::RoundToInt(TargetKg/Mass.ForSource(Sequential.ActiveSupplier))) ||
    FMath::Abs(State.DoseFinalKg-TargetKg)>ToleranceKg+1e-12){
  RejectDose(Revision,FString::Printf(TEXT("계량 범위 벗어남 · 목표 %.9f g ± %.9f g · 확정 %.9f g · 차이 %+.9f g"),TargetKg*1000,ToleranceKg*1000,State.DoseFinalKg*1000,(State.DoseFinalKg-TargetKg)*1000));return false;}
 SelectedDose.Init(0,IsA.Num());for(int32 I:WeighingIds)SelectedDose[I]=1;
 State.TotalCount=WeighingIds.Num();State.TotalKg=State.DoseFinalKg;State.DoseFinalized=true;State.DoseFinalizeRevision=Revision;
 auto& Material=Sequential.Materials[Sequential.ActiveSupplier];
 Material.TargetKg=TargetKg;Material.ToleranceKg=ToleranceKg;Material.FinalKg=State.DoseFinalKg;Material.FinalSimulationSeconds=Time;
 Cohorts[Sequential.ActiveSupplier]=SelectedDose;
 State.DoseReason=TEXT("작업자 최종 계량 확정 · 허용 범위 내 실제 입자만 이송 대상으로 고정");
 Observe(P,Time);return State.Valid;
}

bool FPlantSupplyABatch::Transition(const std::vector<powder::Grain>& P,double Time,int64 CaptureRevision,int64 Revision,bool Combine,int32 Supplier)
{
 if(Revision>0&&Sequential.TransitionRevision==Revision)return true;
 if(Revision>0&&Sequential.RejectedRevision==Revision)return false;
 Observe(P,Time);
 if(!State.Valid||Revision<=0||CaptureRevision<=0||State.CaptureRevision!=CaptureRevision||!State.DoseFinalized||
    State.TotalCount<=0||State.ReceiverCount!=State.TotalCount||State.ForeignCount||State.UnknownCount||
    Sequential.Combined||!Recovery.ForeignIds.IsEmpty()){
  RejectTransition(Revision,TEXT("원료 전환/합산 직전 확정 입자 전량 수취·공용 경로 비움 미확인"));return false;}
 if(!Combine&&(!Sequential.Materials.IsValidIndex(Supplier)||!Cohorts[Supplier].IsEmpty()||
    Sequential.Materials[Supplier].InitialCount<=0||Sequential.Materials[Supplier].SourceCount!=Sequential.Materials[Supplier].InitialCount)){
  RejectTransition(Revision,TEXT("다음 원료 상류 재고 또는 중복 공급 확인 필요"));return false;
 }
 Sequential.Materials[Sequential.ActiveSupplier].ReceiverCompleteSeconds=Time;
 if(!Combine)SelectSource(Supplier);
 else{
  int32 Count=0;double Kg=0;
  for(const auto& Material:Sequential.Materials){
   if(Material.SelectedCount!=Material.ReceiverCount){RejectTransition(Revision,TEXT("선행 계량분 수취 보관 불일치"));return false;}
   Count+=Material.SelectedCount;Kg+=Material.FinalKg;
  }
  Sequential.Combined=true;SelectedDose.Init(0,Labels.Num());IsA.Init(0,Labels.Num());
  for(int32 I=0;I<Labels.Num();++I){
   int32 Label=Labels[I];if(!Sequential.Materials.IsValidIndex(Label)||Cohorts[Label].IsEmpty())continue;
   IsA[I]=1;SelectedDose[I]=Cohorts[Label][I];
  }
  State.TotalCount=Count;State.TotalKg=State.DoseFinalKg=Kg;
 }
 State.CaptureRevision=Revision;State.CapturedSimulationSeconds=Time;WeighingIds.Empty();WeighingStableSince=-1;
 Sequential.TransitionRevision=Revision;Sequential.Reason=Combine?TEXT("선행 계량분 합산 · 전량 수취 확인"):TEXT("수취 보관 확인 · 다음 공급원 확정");
 Observe(P,Time);return State.Valid&&State.ForeignCount==0&&State.UnknownCount==0;
}

bool FPlantSupplyABatch::BeginAdditive(const std::vector<powder::Grain>& P,double Time,int64 CaptureRevision,int64 Revision,bool Scheduled,double Delay,int32 Supplier)
{
 if(Revision>0&&Sequential.TransitionRevision==Revision)return Sequential.AdditiveActive;
 if(Revision>0&&Sequential.RejectedRevision==Revision)return false;
 Observe(P,Time);
 if(!State.Valid||Revision<=0||State.CaptureRevision!=CaptureRevision||CaptureRevision<=0||
    !Sequential.Combined||Sequential.AdditiveActive||State.MixerCount!=State.TotalCount||State.TotalCount<=0||
    State.ForeignCount||State.UnknownCount||Feed.State!=EPlantTransferState::Complete||
    Feed.CloseRevision<=Feed.OpeningRevision||Feed.ClosedSimulationSeconds<0||
    !Sequential.Materials.IsValidIndex(Supplier)||!Cohorts[Supplier].IsEmpty()||Sequential.Materials[Supplier].InitialCount<=0||Sequential.Materials[Supplier].SourceCount!=Sequential.Materials[Supplier].InitialCount||
    !FMath::IsFinite(Delay)||Delay<0){
  RejectTransition(Revision,TEXT("첨가제 준비 시작 금지 · 일반 원료 전량 도착/수취 닫힘/첨가제 초기 재고 미확인"));return false;
 }
 InitialFeed=Feed;
 SelectSource(Supplier);Sequential.AdditiveActive=true;Sequential.AdditiveScheduled=Scheduled;Sequential.AdditiveDelaySeconds=Delay;

 ExistingMixer.Init(0,Labels.Num());UnexpectedSeen.Init(0,Labels.Num());int32 Existing=0;
 for(int32 I=0;I<Labels.Num();++I)if(!IsA[I]&&Geometry.Classify(P[I])==EPlantMixerRegion::Mixer){ExistingMixer[I]=1;++Existing;}

 State.InitialKg=State.TotalKg=State.TotalCount*Mass.ForSource(Sequential.ActiveSupplier);State.InitialMixerCount=Existing;
 State.CaptureRevision=Revision;State.CapturedSimulationSeconds=Time;
 Feed=FPlantMixerFeedSnapshot();Feed.Supported=true;for(int32 I=0;I<ExistingMixer.Num();++I)if(ExistingMixer[I])Feed.InitialMixerKg+=Mass.ForId(I);StableSince=-1;
 WeighingIds.Empty();WeighingStableSince=-1;
 Sequential.TransitionRevision=Revision;Sequential.Reason=TEXT("일반 원료 혼합기 보관 · 첨가제 초기 입자 확정");
 Observe(P,Time);return State.Valid&&State.ForeignCount==0&&State.UnknownCount==0;
}
void FPlantSupplyABatch::ObserveAdditiveReady(double Time,bool ClosedAndVented)
{
 if(!Sequential.AdditiveActive)return;
 const bool Ready=ClosedAndVented&&State.Valid&&State.DoseFinalized&&State.TotalCount>0&&
  State.ReceiverCount==State.TotalCount&&State.UnknownCount==0&&State.ForeignCount==0&&Feed.OpeningRevision==0&&
  Sequential.RegularsInMixer();
 if(Ready&&!Sequential.AdditiveReady){ReadyMixSeconds=Recovery.AutomaticMixSeconds;Sequential.AdditiveReadySeconds=Time;Sequential.Materials[Sequential.ActiveSupplier].ReceiverCompleteSeconds=Time;Sequential.Additive=Sequential.Materials[Sequential.ActiveSupplier];}
 if(!Ready&&Feed.OpeningRevision==0)Sequential.AdditiveReadySeconds=-1;
 Sequential.AdditiveReady=Ready;
}
