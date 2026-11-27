#include "ProcessAutoRun.h"
#include "ProcessRecipe.h"
#include "ProcessStagePressure.h"
#include "ProcessPlantBinding.h"
#include "PlantDoseValidation.h"
#include "PlantDoseTrace.h"

namespace {
bool Fresh(const FPlantSimulationSnapshot& S,double Now){return S.Validity==EPlantValueValidity::Valid&&S.PublishedWallSeconds>0&&Now>=S.PublishedWallSeconds&&Now-S.PublishedWallSeconds<=2.;}
bool PurgeOff(const FPlantSimulationSnapshot& S){for(int32 I=0;I<6;++I){const auto& P=PlantPurgeFor(S,I);if(P.Supported&&(!P.Valid||P.RequestedEnabled||P.AppliedEnabled||P.RequestedGaugePa!=0||P.AppliedGaugePa!=0||P.FlowM3s!=0))return false;}return S.SupplyAPurge.Valid;}
}
FString FProcessAutoRun::StageText(EProcessAutoStage S)
{
 switch(S){
 case EProcessAutoStage::RecoveryAdmit:return TEXT("배치 재개 · 작업자 이력 확인 중");
 case EProcessAutoStage::RecoveryRun:case EProcessAutoStage::RecoveryRoute:case EProcessAutoStage::RecoveryClearPressure:case EProcessAutoStage::RecoveryCloseOutlet:return TEXT("배치 재개 · 남은 경로 준비 중");
 case EProcessAutoStage::RecoveryRPM:case EProcessAutoStage::RecoveryMix:return TEXT("배치 재개 · 남은 혼합 시간 준비 중");
 case EProcessAutoStage::BeginAdditive:return TEXT("첨가제 초기 재고·A·B 혼합기 보관 확인");
 case EProcessAutoStage::WaitAdditive:return TEXT("첨가제 수취 대기 · 예약 시각 확인");
 case EProcessAutoStage::BeginB:return TEXT("선행 원료 보관 확인 · 다음 계량분 준비");case EProcessAutoStage::CombineAB:return TEXT("원료별 전량 수취 확인 · 합산 배치 확정");
 case EProcessAutoStage::Idle:return TEXT("시작 전");case EProcessAutoStage::InitialStop:return TEXT("초기 정지 확인");
 case EProcessAutoStage::CaptureBatch:return TEXT("A 초기 배치 확정");case EProcessAutoStage::Resume:return TEXT("계산 재개 확인");
 case EProcessAutoStage::FeedReceiverBaseline:case EProcessAutoStage::PurgeReceiverBaseline:case EProcessAutoStage::PurgeWeigherBaseline:case EProcessAutoStage::SupplyPressure:case EProcessAutoStage::FeedPressure:return TEXT("공급 압력 적용 확인");
 case EProcessAutoStage::OpenA:return TEXT("공급 열림 적용 확인");case EProcessAutoStage::Feeding:return TEXT("현재 원료 공급 중");
 case EProcessAutoStage::CloseA:return TEXT("공급 실제 닫힘 확인");case EProcessAutoStage::SupplyZero:return TEXT("공급 압력 0 적용 확인");
 case EProcessAutoStage::PurgePressure:case EProcessAutoStage::PurgeStart:return TEXT("연결부 공기 적용 확인");
 case EProcessAutoStage::Clearing:return TEXT("현재 공급 연결부 비우는 중");case EProcessAutoStage::PurgeStop:return TEXT("연결부 공기 차단·압력 0·유량 0 확인");
 case EProcessAutoStage::WeighingStable:return TEXT("칭량 구성·질량 안정 확인");
 case EProcessAutoStage::FinalizeDose:return TEXT("작업자 최종 계량 확정 중");
 case EProcessAutoStage::TransferPressure:case EProcessAutoStage::ReceiverPressure:return TEXT("수취 이송 압력 적용 확인");
 case EProcessAutoStage::OpenWeigher:return TEXT("공급 재확인·기준량 확정·칭량 출구 열림 확인");
 case EProcessAutoStage::Transferring:return TEXT("수취 도착·이송 완료 관찰");case EProcessAutoStage::CloseWeigher:return TEXT("이송 완료 · 칭량 출구 닫힘 확인");
 case EProcessAutoStage::VentWeigher:case EProcessAutoStage::VentReceiver:return TEXT("혼합 투입 전 압력 0 적용 확인");
 case EProcessAutoStage::OpenReceiver:return TEXT("수취 배치 재확인·출구 열림 확인");
 case EProcessAutoStage::FeedingMixer:return TEXT("혼합기로 투입 중");
 case EProcessAutoStage::CloseReceiver:return TEXT("수취호퍼 출구 닫힘 확인 중");
 case EProcessAutoStage::SetMixerRPM:case EProcessAutoStage::StartMixer:return TEXT("혼합 시작 확인 중");
 case EProcessAutoStage::Mixing:return TEXT("혼합 중");
 case EProcessAutoStage::StopMixer:return TEXT("혼합기 정지 확인 중");
 case EProcessAutoStage::Complete:return TEXT("배치 완료");case EProcessAutoStage::Stopping:return TEXT("실패 · 전체 정지 확인 중");
 case EProcessAutoStage::Cancelled:return TEXT("자동 실행 중단 · 배치 기록 보존");case EProcessAutoStage::Failed:return TEXT("자동 실행 실패");default:return TEXT("미확인");}
}
bool FProcessAutoRun::SetMixingDuration(double Seconds,FString& Reason)
{
 if(State.Active){Reason=TEXT("진행 중인 배치의 혼합 시간은 변경할 수 없습니다.");return false;}
 if(!FMath::IsFinite(Seconds)||Seconds<=0){Reason=TEXT("혼합 시간은 초 단위의 유한한 양수여야 합니다. 적용 값은 변경하지 않았습니다.");return false;}
 Settings.MixingDurationSeconds=Seconds;Reason.Empty();return true;
}
FString FProcessAutoRun::CanStart(const FProcessControlMachine& M,double Now) const
{
 if(State.Active)return TEXT("현재 배치의 자동 실행 중입니다.");
 const auto PressureWhy=ProcessStagePressure::Validate(Settings.FeedPressurePa,Settings.TransferPressurePa,Settings.SupplierFeedPa);if(!PressureWhy.IsEmpty())return PressureWhy;
 const auto& S=M.Snapshot;const auto& B=S.SupplyABatch;
 if(!M.Connected||!Fresh(S,Now)||S.GPUDevice.IsEmpty())return TEXT("실제 GPU 작업자의 최신 확인값을 기다리세요.");
 if(!ProcessPlantBinding::IsLowPoly(S.SourceActorPath)||!B.Supported||!S.SupplyAPurge.Supported||!S.FeedPreparation.Enabled)return TEXT("LowPoly 공급·칭량 자동 운전 구성을 확인하세요.");
 if(!S.MixerFeed.Supported||!S.MixerFeed.Valid)return TEXT("현재 수취→혼합기 구역·배치 관찰을 확인할 수 없습니다.");
 if(!FMath::IsFinite(ConfiguredRPM(S))||ConfiguredRPM(S)<=0||ConfiguredRPM(S)>PlantControl::MaxRPM)return TEXT("자동 시작 전 혼합 목표 RPM을 유효한 양수(최대30)로 설정하세요.");
 if(!FMath::IsFinite(Settings.MixingDurationSeconds)||Settings.MixingDurationSeconds<=0)return TEXT("혼합기 패널에서 양수 혼합 시간(초)을 적용한 뒤 시작하세요.");
 if(M.IsStopping()||M.StopStage==EProcessStopStage::Failed)return TEXT("전체 정지의 적용 결과를 먼저 확인하세요.");
 for(const auto& Pair:M.History)if(Pair.Value.IsPending())return TEXT("이전 명령 적용 확인 중입니다.");
 if(!Settings.Entries.IsEmpty()){
  const auto PlanWhy=ProcessRecipe::ValidateEntries(Settings.Entries,Settings.AdditiveEnabled);
  if(!PlanWhy.IsEmpty())return PlanWhy;
  if(Settings.BatchMode!=EProcessBatchMode::SequentialAB)return TEXT("호퍼 목록의 순차 계량 설정을 확인하세요.");
  if(Settings.AdditiveScheduled&&(!Settings.AdditiveEnabled||!FMath::IsFinite(Settings.AdditiveDelaySeconds)||Settings.AdditiveDelaySeconds<0||Settings.AdditiveDelaySeconds>=Settings.MixingDurationSeconds))return TEXT("첨가제 예약 시각과 혼합 시간을 확인하세요.");
  if(!S.DoseCutoff.Supported)return TEXT("해석 작업자의 목표량 차단 기능 미지원");
  if(!B.Valid||B.CaptureRevision||B.ForeignCount||B.UnknownCount)return TEXT("새 배치의 초기 입자 위치를 확인하세요.");
  for(const auto& E:Settings.Entries){
   if(!E.Enabled)continue;
   if(E.Additive&&(Settings.AdditiveSupplier!=E.Supplier||Settings.AdditiveTargetKg!=E.TargetKg||Settings.AdditiveToleranceKg!=E.ToleranceKg))return TEXT("첨가제 호퍼·목표량과 레시피가 일치하지 않습니다.");
   const FString Prefix=FString(ProcessPlantBinding::SupplierId(E.Supplier))+TEXT(" · ");
   if(!S.SequentialBatch.Materials.IsValidIndex(E.Supplier)||!PlantPurgeFor(S,E.Supplier).Valid||!PlantBrakeFor(S,E.Supplier).Valid)return Prefix+TEXT("제동·연결부 공기 또는 해석 위치 미확인");
   const auto& Mat=S.SequentialBatch.Materials[E.Supplier];
   if(Mat.InitialCount==0)return Prefix+TEXT("장입 원료 없음");
   if(Mat.InitialCount!=Mat.SourceCount)return Prefix+TEXT("원료 상류 보존 미확인");
   const auto Why=PlantDose::Validate(E.TargetKg,E.ToleranceKg,S.ModelParticleMassKg,Mat.SourceCount*S.ModelParticleMassKg);
   if(!Why.IsEmpty())return Prefix+Why;
  }
 }else{
 if(!B.Valid||B.CaptureRevision||B.TotalCount<=0||B.SourceCount!=B.TotalCount||B.ForeignCount||B.UnknownCount)
   return TEXT("초기 A 원료 전량이 공급호퍼에 있는 새 배치가 필요합니다. 초기화 후 확인하세요.");
 if(Settings.BatchMode!=EProcessBatchMode::Full){
  const auto Why=PlantDose::Validate(Settings.TargetMassKg,Settings.WeighingToleranceKg,S.ModelParticleMassKg,B.TotalKg);if(!Why.IsEmpty())return Why;
 }
 if(Settings.BatchMode==EProcessBatchMode::SequentialAB){
  const auto& BMaterial=S.SequentialBatch.B;
  if(!S.SupplyBBrake.Supported||!S.SupplyBBrake.Valid||!S.SupplyBPurge.Supported||!S.SupplyBPurge.Valid||BMaterial.SourceCount!=BMaterial.InitialCount)return TEXT("B 상류 재고·제동·하류 공기 구성 미확인");
  const auto Why=PlantDose::Validate(Settings.TargetMassBKg,Settings.WeighingToleranceBKg,S.ModelParticleMassKg,BMaterial.SourceCount*S.ModelParticleMassKg);if(!Why.IsEmpty())return TEXT("B: ")+Why;
 }
 if(Settings.AdditiveEnabled){
  if(Settings.BatchMode!=EProcessBatchMode::SequentialAB||Settings.AdditiveSupplier!=2)return TEXT("첨가제는 A·B 순차 배치의 C 호퍼만 지원합니다.");
  if(!FMath::IsFinite(Settings.AdditiveDelaySeconds)||Settings.AdditiveDelaySeconds<0||
     (Settings.AdditiveScheduled&&Settings.AdditiveDelaySeconds>=Settings.MixingDurationSeconds))return TEXT("예약 시각 N은 0 이상이며 혼합 종료 시각보다 앞서야 합니다.");
  const auto& C=S.SequentialBatch.Additive;
  if(!S.SupplyCBrake.Supported||!S.SupplyCBrake.Valid||!S.SupplyCPurge.Supported||!S.SupplyCPurge.Valid||C.SourceCount!=C.InitialCount)return TEXT("첨가제 C 초기 재고·제동·하류 공기 구성 미확인");
  const auto Why=PlantDose::Validate(Settings.AdditiveTargetKg,Settings.AdditiveToleranceKg,S.ModelParticleMassKg,C.SourceCount*S.ModelParticleMassKg);
  if(!Why.IsEmpty())return TEXT("첨가제 C: ")+Why;
 }
 }
 bool Requested=false;for(bool Open:S.RequestedSuppliers)Requested|=Open;
 if(S.AppliedSupplierMask||Requested||S.RequestedHopperOpen||S.AppliedHopperOpen||S.RequestedReceiverOpen||S.AppliedReceiverOpen||
    S.WeighingMass.Validity!=EPlantValueValidity::Valid||S.PipeMass.Validity!=EPlantValueValidity::Valid||S.ReceiverMass.Validity!=EPlantValueValidity::Valid||
    S.WeighingMass.Kg!=0||S.PipeMass.Kg!=0||S.ReceiverMass.Kg!=0||!PurgeOff(S))
   return TEXT("초기 밸브 모두 닫힘·공기 차단 및 빈 칭량/이송 배관/수취호퍼를 확인하세요.");
 for(double V:{Settings.StageWallSeconds,Settings.FeedSimulationSeconds,Settings.PurgeSimulationSeconds,Settings.TransferSimulationSeconds,Settings.MixerFeedSimulationSeconds,Settings.StagnationSimulationSeconds})
  if(!FMath::IsFinite(V)||V<=0)return TEXT("자동 실행 시험 제한 시간 설정이 유효하지 않습니다.");
 return FString();
}
bool FProcessAutoRun::Start(FGuid Id,FProcessControlMachine& M,double Now)
{
 if(State.Active||Id==State.RunId)return State.Active; // duplicates never replace the batch or step
 const auto Why=CanStart(M,Now);if(!Why.IsEmpty()){State.Reason=Why;return false;}
 State=FProcessAutoSnapshot();State.RunId=Id;State.Epoch=M.Snapshot.Epoch;State.Connection=M.Connection;
 State.LockedSupplierFeedPa=Settings.SupplierFeedPa;State.LockedFeedPressurePa=Settings.FeedPressurePa;State.LockedTransferPressurePa=Settings.TransferPressurePa;
 State.BatchMode=Settings.BatchMode;State.TargetMassKg=Settings.TargetMassKg;State.WeighingToleranceKg=Settings.WeighingToleranceKg;
 State.LockedTargetAKg=Settings.TargetMassKg;State.LockedToleranceAKg=Settings.WeighingToleranceKg;State.LockedTargetBKg=Settings.TargetMassBKg;State.LockedToleranceBKg=Settings.WeighingToleranceBKg;
 State.AdditiveEnabled=Settings.AdditiveEnabled;State.AdditiveSupplier=Settings.AdditiveSupplier;State.AdditiveScheduled=Settings.AdditiveScheduled;
 State.AdditiveTargetKg=Settings.AdditiveTargetKg;State.AdditiveToleranceKg=Settings.AdditiveToleranceKg;State.AdditiveDelaySeconds=Settings.AdditiveScheduled?Settings.AdditiveDelaySeconds:0;
 State.Entries=Settings.Entries;State.FinalBySupplierKg.Init(-1,6);
 if(!State.Entries.IsEmpty()){
  State.ActiveSupplier=State.NextRegular(INDEX_NONE);
  State.TargetMassKg=State.DoseFor(State.ActiveSupplier,false);State.WeighingToleranceKg=State.DoseFor(State.ActiveSupplier,true);
 }
 LastClockSequence=-1;
 State.InitialAvailableKg=State.Entries.IsEmpty()?M.Snapshot.SupplyABatch.TotalKg:M.Snapshot.SequentialBatch.Materials[State.ActiveSupplier].SourceCount*M.Snapshot.ModelParticleMassKg;PreviousPassedKg=0;PreviousPassedTime=-1;LastWeighingSetRevision=-1;
 State.MixingDurationSeconds=State.MixingRemainingSeconds=Settings.MixingDurationSeconds;
 State.TargetMixerRPM=ConfiguredRPM(M.Snapshot);State.Active=true;State.StartedSimulationSeconds=M.Snapshot.PhysicalSeconds;M.AutomationActive=true;
 Enter(EProcessAutoStage::InitialStop,M.Snapshot,Now);return true;
}
void FProcessAutoRun::Enter(EProcessAutoStage Next,const FPlantSimulationSnapshot& S,double Now)
{
 EndReceiverPressure();
 State.PressureStageToken.Invalidate();
 if(Next==EProcessAutoStage::Feeding||Next==EProcessAutoStage::Transferring){
  State.PressureStageToken=FGuid::NewGuid();
  State.ExpectedPressurePa=Next==EProcessAutoStage::Feeding?State.FeedBaseline():State.LockedTransferPressurePa;
  State.PressureEquipment.Empty();State.ReceiverPressureRequest.Invalidate();State.ReceiverPressureStatus.Empty();
 }
 State.Stage=Next;State.PendingRequest.Invalidate();State.Reason=StageText(Next);
 EnteredWall=Now;EnteredSim=ProgressSim=S.PhysicalSeconds;ProgressKey.Empty();StableSince=-1;LastSequence=-1;
 UE_LOG(LogTemp,Display,TEXT("AUTO_BATCH run=%s supplier=%d stage=%d time=%.9f batch_kg=%.12g reason=%s"),*State.RunId.ToString(),State.ActiveSupplier,int32(Next),S.PhysicalSeconds,State.BatchKg,*State.Reason);
}
void FProcessAutoRun::ConfirmMixStart()
{
 State.ConfirmedMixRun=State.MixerRequestRevision;
 State.ConfirmedMixRPM=State.TargetMixerRPM;
 State.SpeedRequest.Invalidate();
}
FString FProcessAutoRun::MixSpeedReason(const FProcessControlMachine& M,double Now) const
{
 const auto& S=M.Snapshot;
 if(M.IsStopping()||State.Stage==EProcessAutoStage::Stopping||State.Stage==EProcessAutoStage::StopMixer)return TEXT("혼합기 정지 확인 중");
 if(!State.Active||State.ConfirmedMixRun<=0)return TEXT("자동 혼합 시작 적용 후 조절할 수 있습니다.");
 if(!M.Connected||State.Connection!=M.Connection||State.Epoch!=S.Epoch||!Fresh(S,Now)||!S.Error.IsEmpty())return TEXT("최신 혼합기 작업자 상태 확인 중");
 if(!M.ManualGranted)return TEXT("작업자 명령 결과를 확인한 뒤 전체 정지 상태를 확인하세요.");
 if(S.MixRun!=State.ConfirmedMixRun||S.Execution!=EPlantExecutionState::Running||!S.RequestedRunning||
    !S.WorkerMixerEnabled||!S.RequestedMixerEnabled||!S.AppliedRPMValid||S.AppliedRPM<=0)return TEXT("혼합기의 실제 회전 확인 중");
 if(MixElapsed(S)>=State.MixingDurationSeconds||S.PhysicalSeconds>=State.MixingDeadlineSimulationSeconds)return TEXT("설정 혼합 시간이 종료되었습니다.");
 if(const auto* R=M.History.Find(State.SpeedRequest);R&&R->IsPending())return TEXT("RPM 적용 확인 중");
 return FString();
}
bool FProcessAutoRun::MixSpeedMatches(const FProcessControlMachine& M) const
{
 const auto& S=M.Snapshot;
 const auto* R=M.History.Find(State.SpeedRequest);
 const double Confirmed=State.ConfirmedMixRPM>0?State.ConfirmedMixRPM:State.TargetMixerRPM;
 const bool Scoped=R&&R->Revision>0&&R->SpeedRun==State.ConfirmedMixRun&&R->SpeedBatch==State.RunId;
 if(State.Stage==EProcessAutoStage::StopMixer&&S.MixerDrive.Supported){
  if(!S.AppliedRPMValid||S.MixerDrive.Tripped)return false;
  if(S.MixerDrive.TargetRPM==0)return true;
  // A published batch may still precede the newly issued stop request.
  if(State.MixerStopRequestRevision>0&&S.ProcessedControlRevision>=State.MixerStopRequestRevision)return false;
 }
 const bool Previous=S.MixerTargetApplied(Confirmed)&&S.WorkerTargetRPM==Confirmed;
 const bool New=Scoped&&S.MixRun==R->SpeedRun&&S.SpeedAppliedRevision==R->Revision&&S.MixerTargetApplied(R->Value)&&S.WorkerTargetRPM==R->Value;
 // Stop releases the request target immediately. Only worker rotation matters during rundown.
 return (Previous||New)&&(State.Stage==EProcessAutoStage::StopMixer||S.RequestedRPM==Confirmed||(Scoped&&S.RequestedRPM==R->Value));
}
void FProcessAutoRun::Cancel(const FString& Reason)
{
 if(!State.Active)return;
 State.ConfirmedMixRun=0;State.SpeedRequest.Invalidate();
 EndReceiverPressure();State.PressureStageToken.Invalidate();
 State.RecoveryPending=State.BatchRevision>0;State.InterruptedStage=State.Stage;State.StopConfirmedSeconds=-1;
 State.Active=false;State.Stage=EProcessAutoStage::Cancelled;State.PendingRequest.Invalidate();State.Reason=Reason;State.AdditiveStatus=TEXT("취소 · 재개 후 자동 복원 안 함");
}
void FProcessAutoRun::ConfirmUI(int64 Epoch,int64 TransferId,double Time)
{
 if(State.Active&&State.Stage==EProcessAutoStage::Transferring&&State.Epoch==Epoch&&State.TransferId==TransferId&&State.UICompleteSimulationSeconds<0)
  State.UICompleteSimulationSeconds=Time;
}
void FProcessAutoRun::Fail(const FString& Why,FProcessControlMachine& M,double Now,const FProcessAutoCommand& Send)
{
 State.ConfirmedMixRun=0;State.SpeedRequest.Invalidate();
 State.InterruptedStage=State.Stage;State.RecoveryPending=State.BatchRevision>0;State.StopRequestedSeconds=M.Snapshot.PhysicalSeconds;State.StopConfirmedSeconds=-1;
 Enter(EProcessAutoStage::Stopping,M.Snapshot,Now);State.Reason=Why;
 const auto R=Send(EProcessCommand::StopAll,TEXT("plant"),0);State.PendingRequest=R.RequestId;
}
bool FProcessAutoRun::Command(EProcessCommand C,const TCHAR* Id,double V,EProcessAutoStage Next,
 FProcessControlMachine& M,double Now,const FProcessAutoCommand& Send)
{
 if(!State.PendingRequest.IsValid()){const auto R=Send(C,Id,V);State.PendingRequest=R.RequestId;
  if(C==EProcessCommand::Valve&&(ProcessPlantBinding::SupplierIndex(FString(Id))>=0)&&V==0&&State.BatchMode!=EProcessBatchMode::Full){
   State.DoseCloseRequestRevision=R.Revision;State.CutoffRequestedSeconds=M.Snapshot.PhysicalSeconds;
   State.CutoffWeighingKg=M.Snapshot.SupplyABatch.WeighingCount*M.Snapshot.ModelParticleMassKg;
   State.CutoffConnectorKg=M.Snapshot.SupplyABatch.ConnectorCount*M.Snapshot.ModelParticleMassKg;
   UE_LOG(LogTemp,Display,TEXT("A_DOSE_CUTOFF revision=%lld time=%.9f target_kg=%.12g estimate_kg=%.12g lead_kg=%.12g drainable_kg=%.12g weighing_kg=%.12g connector_kg=%.12g"),R.Revision,State.CutoffRequestedSeconds,State.TargetMassKg,State.CutoffEstimatedKg,State.CutoffLeadKg,State.CutoffDrainableKg,State.CutoffWeighingKg,State.CutoffConnectorKg);
  }
  if(C==EProcessCommand::Valve&&FString(Id)==TEXT("receiver")&&V==1&&State.AdditiveEnabled&&State.IsAdditive()){
   State.AdditiveOpenRequestedSeconds=M.Snapshot.PhysicalSeconds;State.AdditiveOpenRevision=R.Revision;
  }
  if(C==EProcessCommand::MixerEnabled&&V==1){State.MixerRequestedSimulationSeconds=M.Snapshot.PhysicalSeconds;State.MixerRequestRevision=R.Revision;}
  if(C==EProcessCommand::MixerEnabled&&V==0&&State.Stage==EProcessAutoStage::StopMixer){
   State.MixerStopRequestedSimulationSeconds=M.Snapshot.PhysicalSeconds;State.MixerStopRequestRevision=R.Revision;State.MixerStopRequestId=R.RequestId;
   UE_LOG(LogTemp,Display,TEXT("AUTO_A_TIMED_STOP request=%s epoch=%lld revision=%lld time=%.9f duration=%.9f elapsed=%.9f"),*R.RequestId.ToString(),State.Epoch,R.Revision,State.MixerStopRequestedSimulationSeconds,State.MixingDurationSeconds,State.MixingElapsedSeconds);
  }
  return false;}
 const auto* R=M.History.Find(State.PendingRequest);
 if(!R||R->Epoch!=State.Epoch||R->Connection!=State.Connection){Fail(TEXT("자동 명령 연결·초기화 세대가 변경되었습니다."),M,Now,Send);return false;}
 if(R->IsPending()){State.Reason=StageText(State.Stage)+TEXT(" · ")+R->Message;return false;}
 if(R->Status!=EProcessCommandStatus::Applied){
  const FString Context=C==EProcessCommand::Valve&&(ProcessPlantBinding::SupplierIndex(FString(Id))>=0)&&V==0&&State.BatchMode!=EProcessBatchMode::Full?
   FString(TEXT("목표량 차단 실패 · 실제 닫힘 미확인 · ")):FString();
  Fail(Context+R->Message,M,Now,Send);return false;
 }
 if(C==EProcessCommand::Valve&&(ProcessPlantBinding::SupplierIndex(FString(Id))>=0)&&V==0&&State.BatchMode!=EProcessBatchMode::Full&&
    M.Snapshot.SupplyABatch.SupplyClosedRevision!=R->Revision){Fail(TEXT("공급 실제 닫힘 계량 기록의 작업자 응답 불일치"),M,Now,Send);return false;}
 if(C==EProcessCommand::FinalizeADose){const auto& B=M.Snapshot.SupplyABatch;
  State.DoseFinalized=true;State.DoseFinalizeRevision=R->Revision;State.FinalDoseKg=B.DoseFinalKg;State.FinalDoseSimulationSeconds=B.DoseFinalSimulationSeconds;
  State.BatchCount=B.TotalCount;State.BatchKg=B.TotalKg;
  if(State.FinalBySupplierKg.IsValidIndex(State.ActiveSupplier))State.FinalBySupplierKg[State.ActiveSupplier]=B.DoseFinalKg;
  if(State.IsAdditive())State.FinalAdditiveKg=B.DoseFinalKg;else if(State.ActiveSupplier==0)State.FinalAKg=B.DoseFinalKg;else if(State.ActiveSupplier==1)State.FinalBKg=B.DoseFinalKg;
 }
 if(C==EProcessCommand::BeginB||C==EProcessCommand::CombineAB||C==EProcessCommand::BeginAdditive){
  State.BatchRevision=R->Revision;State.BatchCount=M.Snapshot.SupplyABatch.TotalCount;State.BatchKg=M.Snapshot.SupplyABatch.TotalKg;
  if(C==EProcessCommand::BeginB||C==EProcessCommand::BeginAdditive){
   const bool Add=C==EProcessCommand::BeginAdditive;
   State.ActiveSupplier=M.Snapshot.SequentialBatch.ActiveSupplier;State.TargetMassKg=State.DoseFor(State.ActiveSupplier,false);State.WeighingToleranceKg=State.DoseFor(State.ActiveSupplier,true);
   if(Add){State.AdditivePreparationStartedSeconds=M.Snapshot.SupplyABatch.CapturedSimulationSeconds;State.AdditiveStatus=TEXT("첨가제 개별 계량 준비 중");State.DownstreamStarted=false;}
   State.DoseFinalized=false;State.DoseFinalizeRevision=State.DoseCloseRequestRevision=0;State.FinalDoseKg=0;State.FinalDoseSimulationSeconds=-1;
   State.CutoffEstimatedKg=State.CutoffLeadKg=State.CutoffDrainableKg=0;State.CutoffRequestedSeconds=-1;
   State.ConnectorEmptySimulationSeconds=State.BaselineSimulationSeconds=State.OutletOpenSimulationSeconds=State.FirstArrivalSimulationSeconds=State.UICompleteSimulationSeconds=-1;
   State.TransferId=0;State.RemainingKg=State.TransitKg=State.ArrivedKg=0;State.PreparationStableSeconds=0;
   PreviousPassedKg=0;PreviousPassedTime=-1;LastWeighingSetRevision=-1;State.InitialAvailableKg=State.BatchKg;
  }else State.DownstreamStarted=true;
 }
 if(C==EProcessCommand::Valve&&FString(Id)==TEXT("receiver")&&V==1&&State.AdditiveEnabled&&State.IsAdditive()){
  State.AdditiveOpenAppliedSeconds=M.Snapshot.MixerFeed.StartedSimulationSeconds;State.AdditiveStatus=TEXT("첨가제 혼합기 투입 중");
  if(M.Snapshot.MixerFeed.OpeningRevision!=R->Revision||(State.AdditiveScheduled&&State.AdditiveOpenAppliedSeconds<State.AdditiveDueSeconds)){
   Fail(TEXT("첨가제 개방 작업자 revision/예약 시각 확인 실패"),M,Now,Send);return false;
  }
 }
 if(C==EProcessCommand::CaptureABatch){State.BatchRevision=R->Revision;State.BatchCount=M.Snapshot.SupplyABatch.TotalCount;State.BatchKg=M.Snapshot.SupplyABatch.TotalKg;}
 if(C==EProcessCommand::Valve&&FString(Id)==TEXT("receiver")&&V==0){State.ReceiverCloseRevision=R->Revision>0?R->Revision:M.Snapshot.MixerFeed.CloseRevision;State.ReceiverClosedSimulationSeconds=M.Snapshot.MixerFeed.ClosedSimulationSeconds;}
 Enter(Next,M.Snapshot,Now);return true;
}
bool FProcessAutoRun::WatchProgress(const FPlantSimulationSnapshot& S,double Limit,FString& Why)
{
 const auto& B=S.SupplyABatch;const FString Key=FString::Printf(TEXT("%d/%d/%d/%d/%d/%d/%d"),B.SourceCount,B.ConnectorCount,B.WeighingCount,B.PipeCount,B.ReceiverCount,B.MixerConnectorCount,B.MixerCount);
 if(Key!=ProgressKey){ProgressKey=Key;ProgressSim=S.PhysicalSeconds;}
 if(S.PhysicalSeconds-EnteredSim>Limit){Why=TEXT("단계 해석 시간 제한 초과 · 압력 증가 없이 전체 정지합니다.");return false;}
 if(S.PhysicalSeconds-ProgressSim>Settings.StagnationSimulationSeconds){Why=TEXT("잔량 분포가 시험 관찰 시간 동안 줄지 않습니다. 물리 막힘 확정이 아닌 정체 관찰로 중단합니다.");return false;}
 return true;
}
void FProcessAutoRun::Tick(FProcessControlMachine& M,double Now,const FProcessAutoCommand& Send)
{
 if(!State.Active){ObserveRecovery(M);M.AutomationActive=false;return;}
 const auto& S=M.Snapshot;const auto& B=S.SupplyABatch;
 if(!M.Connected||State.Epoch!=S.Epoch||State.Connection!=M.Connection){Cancel(TEXT("초기화·해석 Actor·연결 변경으로 자동 실행과 대기 요청을 취소했습니다."));M.AutomationActive=false;return;}
 if(State.Stage==EProcessAutoStage::Stopping){
  State.StopResult=M.StopDescription()+TEXT(" · ")+M.UnconfirmedEquipment();
  const auto* R=M.History.Find(State.PendingRequest);
  if(!R||!R->IsPending()){
   State.Active=false;State.Stage=EProcessAutoStage::Failed;M.AutomationActive=false;
   State.StopResult=R&&R->Status==EProcessCommandStatus::Applied?TEXT("공기 차단·압력 0·밸브 닫힘·RPM 0·작업자 일시정지 확인"):TEXT("전체 정지 미확인 · ")+(R?R->Message:FString(TEXT("응답 없음")))+TEXT(" · ")+M.StopDescription();
  }return;
 }
 if(!Fresh(S,Now)||!B.Valid||!S.Error.IsEmpty()){Fail(TEXT("최신 GPU 데이터 또는 작업자 응답 오류: ")+S.Error+B.Error,M,Now,Send);return;}
 if(State.RecoveryCount>0&&S.Recovery.Ticket>0&&
    (State.Stage==EProcessAutoStage::TransferPressure||State.Stage==EProcessAutoStage::ReceiverPressure||State.Stage==EProcessAutoStage::OpenWeigher)&&
    !S.AppliedHopperOpen&&B.WeighingCount==0&&B.PipeCount==0&&B.ReceiverCount==State.BatchCount&&
    B.ForeignCount==0&&B.UnknownCount==0&&S.Transfer.Validity==EPlantValueValidity::Valid&&S.Transfer.Id==State.TransferId&&
    S.Transfer.State!=EPlantTransferState::Indeterminate&&FMath::Abs(S.Transfer.BaselineKg-State.BatchKg)<=S.Transfer.Criteria.MassErrorKg){
  Enter(EProcessAutoStage::RecoveryCloseOutlet,S,Now);State.Reason=TEXT("재개 준비 중 전량 수취 도착 · 불필요한 개방 취소 · 기존 안정 조건 확인");return;
 }
 if(TickRecovery(M,Now,Send))return;
 if(const auto* R=M.History.Find(State.SpeedRequest);R&&R->Status==EProcessCommandStatus::Applied&&
    R->SpeedRun==State.ConfirmedMixRun&&S.SpeedAppliedRevision==R->Revision)State.ConfirmedMixRPM=R->Value;
 if(TickAdditiveClock(M,Now,Send))return;
 if(Now-EnteredWall>Settings.StageWallSeconds){Fail(TEXT("자동 단계의 벽시계 제한 시간을 초과했습니다."),M,Now,Send);return;}
 const auto* DoseRequest=M.History.Find(State.PendingRequest);
 const bool Finalizing=State.Stage==EProcessAutoStage::FinalizeDose&&State.BatchMode!=EProcessBatchMode::Full&&DoseRequest&&
  DoseRequest->Command==EProcessCommand::FinalizeADose&&DoseRequest->Epoch==State.Epoch&&DoseRequest->Connection==State.Connection&&
  B.DoseFinalized&&B.DoseFinalizeRevision==DoseRequest->Revision&&DoseRequest->Revision>0;
 if(State.Stage==EProcessAutoStage::FinalizeDose&&B.DoseFinalSimulationSeconds>=0){State.FinalDoseKg=B.DoseFinalKg;State.FinalDoseSimulationSeconds=B.DoseFinalSimulationSeconds;}
 const bool Transitioning=DoseRequest&&(State.Stage==EProcessAutoStage::BeginB||State.Stage==EProcessAutoStage::CombineAB||State.Stage==EProcessAutoStage::BeginAdditive)&&
  DoseRequest->Epoch==State.Epoch&&DoseRequest->Connection==State.Connection&&DoseRequest->Revision>0&&
  S.SequentialBatch.TransitionRevision==DoseRequest->Revision&&B.CaptureRevision==DoseRequest->Revision;
 if(State.BatchRevision>0&&!Transitioning&&(B.CaptureRevision!=State.BatchRevision||B.UnknownCount||B.ForeignCount||!S.Recovery.ForeignIds.IsEmpty()||(!Finalizing&&(B.TotalCount!=State.BatchCount||B.TotalKg!=State.BatchKg))))
 {Fail(TEXT("이번 계량분 ID·질량 보존 또는 추가 원료 유입을 확인할 수 없습니다."),M,Now,Send);return;}
 const bool ReceiverStage=State.Stage==EProcessAutoStage::OpenReceiver||State.Stage==EProcessAutoStage::FeedingMixer||State.Stage==EProcessAutoStage::CloseReceiver;
 const bool StartingMixer=State.Stage==EProcessAutoStage::StartMixer||State.Stage==EProcessAutoStage::Mixing||State.Stage==EProcessAutoStage::StopMixer||(State.AdditiveEnabled&&State.MixerAppliedSimulationSeconds>=0);
 if((S.AppliedSupplierMask&~(1<<State.ActiveSupplier))||(!ReceiverStage&&(S.AppliedReceiverOpen||S.RequestedReceiverOpen))||
    (!StartingMixer&&(S.RequestedMixerEnabled||(S.AppliedRPMValid&&S.AppliedRPM!=0))))
 {Fail(TEXT("현재 단계의 공급·수취 밸브/혼합 정지 조건이 바뀌었습니다."),M,Now,Send);return;}
 if(State.DownstreamStarted&&(!S.MixerFeed.Valid||S.MixerFeed.State==EPlantTransferState::Indeterminate))
 {Fail(TEXT("혼합 투입 판정 불가: ")+S.MixerFeed.Reason,M,Now,Send);return;}
 const auto& Purge=PlantPurgeFor(S,State.ActiveSupplier);
 const auto& Brake=PlantBrakeFor(S,State.ActiveSupplier);
 const TCHAR* SupplierId=ProcessPlantBinding::SupplierId(State.ActiveSupplier);
 const bool NewSample=S.Sequence>LastSequence;LastSequence=S.Sequence;FString Why;
 if(NewSample&&State.ActiveSupplier==1&&(State.Stage==EProcessAutoStage::Feeding||State.Stage==EProcessAutoStage::OpenA||State.Stage==EProcessAutoStage::CloseA))PlantDoseTrace::Snapshot(TEXT("controller_read"),S);
 const auto Go=[&](EProcessCommand C,const TCHAR* Id,double V,EProcessAutoStage Next){return Command(C,Id,V,Next,M,Now,Send);};
 switch(State.Stage){
 case EProcessAutoStage::InitialStop:Go(EProcessCommand::StopAll,TEXT("plant"),0,EProcessAutoStage::CaptureBatch);break;
 case EProcessAutoStage::CaptureBatch:Go(EProcessCommand::CaptureABatch,SupplierId,0,EProcessAutoStage::Resume);break;
 case EProcessAutoStage::Resume:Go(EProcessCommand::Resume,TEXT("plant"),1,EProcessAutoStage::SupplyPressure);break;
 case EProcessAutoStage::SupplyPressure:Go(EProcessCommand::Pressure,SupplierId,State.FeedBaseline().X,EProcessAutoStage::FeedPressure);break;
 case EProcessAutoStage::FeedPressure:Go(EProcessCommand::Pressure,TEXT("weigher"),State.LockedFeedPressurePa.Y,S.AppliedGaugePressurePa.Z==State.LockedFeedPressurePa.Z?EProcessAutoStage::OpenA:EProcessAutoStage::FeedReceiverBaseline);break;
 case EProcessAutoStage::FeedReceiverBaseline:Go(EProcessCommand::Pressure,TEXT("receiver"),State.LockedFeedPressurePa.Z,EProcessAutoStage::OpenA);break;
 case EProcessAutoStage::OpenA:Go(EProcessCommand::Valve,SupplierId,1,EProcessAutoStage::Feeding);break;
 case EProcessAutoStage::Feeding:
  if(!State.Entries.IsEmpty()&&!S.DoseCutoff.Supported){Fail(TEXT("작업자 계량 차단 상태 미확인"),M,Now,Send);break;}
  if(State.BatchMode!=EProcessBatchMode::Full&&S.DoseCutoff.Supported){
   const auto& D=S.DoseCutoff;
   if(!D.Valid||D.Cancelled||D.Epoch!=State.Epoch||D.BatchRevision!=State.BatchRevision||D.Supplier!=State.ActiveSupplier||
      D.TargetKg!=State.TargetMassKg||D.ToleranceKg!=State.WeighingToleranceKg){Fail(TEXT("작업자 계량 차단 확인 실패: ")+D.Reason,M,Now,Send);break;}
   State.CutoffEstimatedKg=D.EstimatedKg;State.CutoffDrainableKg=D.SpillKg;State.CutoffLeadKg=D.LeadKg;
   State.Reason=FString::Printf(TEXT("작업자 계량 · 목표 %.6f g · 통과 %.6f g · 차단 추정 %.6f g · 표본 %lld"),State.TargetMassKg*1000,D.PassedKg*1000,D.EstimatedKg*1000,D.Sample);
   if(D.Triggered){Enter(EProcessAutoStage::CloseA,S,Now);Go(EProcessCommand::Valve,SupplierId,0,EProcessAutoStage::SupplyZero);break;}
   if(!D.Armed||S.AppliedSupplierMask!=(1<<State.ActiveSupplier)||!S.AppliedPressureValid||!ObserveReceiverPressure(M,Why)){
    Fail(TEXT("작업자 계량 공급·압력 조건 변경"),M,Now,Send);break;
   }
   if(!WatchProgress(S,Settings.FeedSimulationSeconds,Why))Fail(Why,M,Now,Send);break;
  }
  if(S.AppliedSupplierMask!=(1<<State.ActiveSupplier)||!S.AppliedPressureValid||!ObserveReceiverPressure(M,Why)){Fail(TEXT("현재 공급 밸브·검증 압력의 적용 상태가 바뀌었습니다."),M,Now,Send);break;}
  State.Reason=FString::Printf(TEXT("현재 공급호퍼 잔량 %d / %d개 · 작업자 좌표로 출구 통과 확인 중"),B.SourceCount,State.BatchCount);
  if(State.BatchMode!=EProcessBatchMode::Full&&NewSample){
   const double Passed=(B.WeighingCount+B.ConnectorCount)*S.ModelParticleMassKg;
   if(S.PhysicalSeconds>PreviousPassedTime){
    State.CutoffLeadKg=PreviousPassedTime>=0?FMath::Max(0.,Passed-PreviousPassedKg):0.;
    PreviousPassedKg=Passed;PreviousPassedTime=S.PhysicalSeconds;
   }
   if(Brake.Supported&&!Brake.Valid){Fail(TEXT("선택 공급 피더의 최신 위치·속도 관찰 미확인"),M,Now,Send);break;}
   State.CutoffDrainableKg=Brake.Supported?Brake.PredictedSpillKg:0.;
   State.CutoffEstimatedKg=Passed+State.CutoffDrainableKg+State.CutoffLeadKg;
   State.Reason=FString::Printf(TEXT("목표 %.6f g · 칭량+연결부 %.6f g · 차단 추정 %.6f g (제동 후 배출 예상 입자+한 표본 증가분 포함, 확정값 아님)"),State.TargetMassKg*1000,Passed*1000,State.CutoffEstimatedKg*1000);
   if(State.CutoffEstimatedKg>=State.TargetMassKg||B.SourceCount==0){PlantDoseTrace::Snapshot(TEXT("controller_cutoff"),S,State.CutoffEstimatedKg);Enter(EProcessAutoStage::CloseA,S,Now);Go(EProcessCommand::Valve,SupplierId,0,EProcessAutoStage::SupplyZero);break;}
   if(!WatchProgress(S,Settings.FeedSimulationSeconds,Why))Fail(Why,M,Now,Send);break;
  }
  if(NewSample&&B.SourceCount==0)Enter(EProcessAutoStage::CloseA,S,Now);
  else if(!WatchProgress(S,Settings.FeedSimulationSeconds,Why))Fail(Why,M,Now,Send);break;
 case EProcessAutoStage::CloseA:Go(EProcessCommand::Valve,SupplierId,0,EProcessAutoStage::SupplyZero);break;
 case EProcessAutoStage::SupplyZero:Go(EProcessCommand::Pressure,SupplierId,0,(S.AppliedGaugePressurePa.Z!=0||S.AppliedGaugePressurePa.Y!=-500)?EProcessAutoStage::PurgeReceiverBaseline:EProcessAutoStage::PurgePressure);break;
 case EProcessAutoStage::PurgeReceiverBaseline:Go(EProcessCommand::Pressure,TEXT("receiver"),0,EProcessAutoStage::PurgeWeigherBaseline);break;
 case EProcessAutoStage::PurgeWeigherBaseline:Go(EProcessCommand::Pressure,TEXT("weigher"),-500,EProcessAutoStage::PurgePressure);break;
 case EProcessAutoStage::PurgePressure:Go(EProcessCommand::PurgePressure,SupplierId,500,EProcessAutoStage::PurgeStart);break;
 case EProcessAutoStage::PurgeStart:Go(EProcessCommand::PurgeEnabled,SupplierId,1,EProcessAutoStage::Clearing);break;
 case EProcessAutoStage::Clearing:
  State.Reason=FString::Printf(TEXT("공급 실제 닫힘 · 연결부 %d개 / %.6f g · 적용 공기 %.6g m³/s"),Purge.ResidualCount,Purge.ResidualKg*1000.,Purge.FlowM3s);
  if(!Purge.Valid||S.AppliedSupplierMask||S.AppliedHopperOpen||!Purge.AppliedEnabled||Purge.AppliedGaugePa!=500||Purge.FlowM3s<=0||S.AppliedGaugePressurePa!=FVector(0,-500,0)){Fail(TEXT("연결부 공기/밸브 적용 상태가 바뀌었습니다."),M,Now,Send);break;}
  if(NewSample&&B.ConnectorCount==0&&Purge.ResidualCount==0){State.ConnectorEmptySimulationSeconds=S.PhysicalSeconds;Enter(EProcessAutoStage::PurgeStop,S,Now);}
  else if(!WatchProgress(S,Settings.PurgeSimulationSeconds,Why))Fail(Why,M,Now,Send);break;
 case EProcessAutoStage::PurgeStop:Go(EProcessCommand::PurgeEnabled,SupplierId,0,EProcessAutoStage::WeighingStable);break;
 case EProcessAutoStage::WeighingStable:
  if(!PurgeOff(S)||B.ConnectorCount||B.PipeCount||B.ReceiverCount||B.MixerConnectorCount||B.MixerCount||B.WeighingCount<=0||
     (State.BatchMode==EProcessBatchMode::Full?(B.WeighingCount!=State.BatchCount||B.SourceCount):(B.WeighingCount+B.SourceCount!=State.BatchCount)))
  {Fail(TEXT("공기 차단 후 칭량·상류 잔량의 배치 보존 상태를 확인할 수 없습니다."),M,Now,Send);break;}
  if(NewSample&&S.Execution==EPlantExecutionState::Running){
   if(LastWeighingSetRevision!=B.WeighingSetRevision){StableSince=-1;LastWeighingSetRevision=B.WeighingSetRevision;}
   if(StableSince<0)StableSince=S.PhysicalSeconds;State.PreparationStableSeconds=S.PhysicalSeconds-StableSince;
   if(State.BatchMode!=EProcessBatchMode::Full)State.PreparationStableSeconds=FMath::Min(State.PreparationStableSeconds,B.WeighingStableSeconds);
  }
  State.Reason=FString::Printf(TEXT("현재 원료 동일 입자·질량 안정 %.3f / %.3f 해석초"),State.PreparationStableSeconds,S.Transfer.Criteria.StableSimulationSeconds);
  if(State.PreparationStableSeconds>=S.Transfer.Criteria.StableSimulationSeconds)Enter(State.BatchMode!=EProcessBatchMode::Full?EProcessAutoStage::FinalizeDose:EProcessAutoStage::TransferPressure,S,Now);break;
 case EProcessAutoStage::FinalizeDose:Go(EProcessCommand::FinalizeADose,SupplierId,0,EProcessAutoStage::TransferPressure);break;
 case EProcessAutoStage::TransferPressure:Go(EProcessCommand::Pressure,TEXT("weigher"),State.LockedTransferPressurePa.Y,EProcessAutoStage::ReceiverPressure);break;
 case EProcessAutoStage::ReceiverPressure:Go(EProcessCommand::Pressure,TEXT("receiver"),State.LockedTransferPressurePa.Z,EProcessAutoStage::OpenWeigher);break;
 case EProcessAutoStage::OpenWeigher:
  if(Go(EProcessCommand::Valve,TEXT("weigher"),1,EProcessAutoStage::Transferring)){
   const auto& T=S.Transfer;
   if(T.Id<=0||FMath::Abs(T.BaselineKg-State.BatchKg)>T.Criteria.MassErrorKg){Fail(TEXT("개방 직전 기준량과 확정 계량분 총량 불일치"),M,Now,Send);break;}
   State.ReceiverPressureTransferId=T.Id;State.ReceiverPressureConfirmedPa=State.LockedTransferPressurePa.Z;
   State.ReceiverPressurePreviousPa=State.ReceiverPressureRequestedPa=State.LockedTransferPressurePa.Z;
   State.ReceiverPressureRequest.Invalidate();State.ReceiverPressureRevision=0;State.ReceiverPressureStatus.Empty();
   State.TransferId=T.Id;State.BaselineSimulationSeconds=T.StartedSimulationSeconds;State.OutletOpenSimulationSeconds=S.FeedPreparation.OutletAppliedSimulationSeconds;
  }break;
 case EProcessAutoStage::Transferring:{
  const bool ClosedArrival=State.RecoveryCount>0&&S.Recovery.Ticket>0&&!S.AppliedHopperOpen&&!S.RequestedHopperOpen&&B.WeighingCount==0&&S.Transfer.Id==State.TransferId;
  if(S.AppliedSupplierMask||S.AppliedReceiverOpen||S.RequestedReceiverOpen||!PurgeOff(S)||!S.AppliedPressureValid||
     (!ClosedArrival&&(!S.AppliedHopperOpen||!ObserveReceiverPressure(M,Why)))){Fail(TEXT("이송 압력·칭량 출구·공기 차단의 적용 상태가 바뀌었습니다. ")+Why,M,Now,Send);break;}
  const auto& T=S.Transfer;State.RemainingKg=T.RemainingKg;State.TransitKg=T.InTransitKg;State.ArrivedKg=T.ArrivedKg;State.AdditionalInflowCount=T.AdditionalInflowCount;
  if(T.ArrivedKg>0&&State.FirstArrivalSimulationSeconds<0)State.FirstArrivalSimulationSeconds=S.PhysicalSeconds;
  if(T.Id!=State.TransferId||T.State==EPlantTransferState::Indeterminate||T.AdditionalInflowCount){Fail(TEXT("이송 판정 불가: ")+T.Reason,M,Now,Send);break;}
  State.Reason=T.Reason;
  if(T.State==EPlantTransferState::Complete&&T.RemainingKg<=T.Criteria.WeighingResidualKg&&T.InTransitKg<=T.Criteria.PipeResidualKg&&
     T.UnclassifiedKg==0&&FMath::Abs(T.ArrivedKg-State.BatchKg)<=T.Criteria.MassErrorKg&&T.StableSeconds>=T.Criteria.StableSimulationSeconds){
   // A pressure upload may race the final arrival. Remember only the actual boundary
   // while waiting for the UI completion acknowledgement and closing the outlet.
   State.ExpectedPressurePa=S.AppliedGaugePressurePa;
   State.ExpectedPressurePa.X=S.SupplierPressure(State.ActiveSupplier,true);
   State.ReceiverPressureConfirmedPa=S.AppliedGaugePressurePa.Z;EndReceiverPressure();
   if(State.UICompleteSimulationSeconds>=0)Enter(EProcessAutoStage::CloseWeigher,S,Now);else State.Reason=TEXT("작업자 전량 도착·안정 확인 · UI 완료 표시 확인 중");
  }else if(!WatchProgress(S,Settings.TransferSimulationSeconds,Why))Fail(Why,M,Now,Send);
  break;}
 case EProcessAutoStage::CloseWeigher:
  if(Go(EProcessCommand::Valve,TEXT("weigher"),0,EProcessAutoStage::VentWeigher)){
   if(B.ReceiverCount!=State.BatchCount||S.AppliedHopperOpen||S.RequestedHopperOpen){Fail(TEXT("칭량 닫힘 후 현재 계량분 전량 수취 상태 불일치"),M,Now,Send);break;}
   State.DownstreamStarted=State.BatchMode!=EProcessBatchMode::SequentialAB;
  }break;
 case EProcessAutoStage::VentWeigher:Go(EProcessCommand::Pressure,TEXT("weigher"),0,EProcessAutoStage::VentReceiver);break;
 case EProcessAutoStage::VentReceiver:Go(EProcessCommand::Pressure,TEXT("receiver"),0,
  State.BatchMode==EProcessBatchMode::SequentialAB?(State.IsAdditive()?EProcessAutoStage::WaitAdditive:(State.NextRegular(State.ActiveSupplier)!=INDEX_NONE?EProcessAutoStage::BeginB:EProcessAutoStage::CombineAB)):EProcessAutoStage::OpenReceiver);break;
 case EProcessAutoStage::BeginAdditive:Go(EProcessCommand::BeginAdditive,TEXT("plant"),0,EProcessAutoStage::SupplyPressure);break;
 case EProcessAutoStage::WaitAdditive:
  State.AdditiveStatus=S.SequentialBatch.AdditiveReady?TEXT("첨가제 수취 준비 완료 · 출구 닫힘 대기"):TEXT("첨가제 준비 상태 재확인 중");
  State.Reason=State.AdditiveStatus;
  if(S.SequentialBatch.AdditiveReady){
   State.AdditivePreparedSeconds=S.SequentialBatch.AdditiveReadySeconds;
   if(!State.AdditiveScheduled)Enter(EProcessAutoStage::OpenReceiver,S,Now);
   else if(State.MixerAppliedSimulationSeconds<0)Enter(EProcessAutoStage::SetMixerRPM,S,Now);
   else if(S.PhysicalSeconds>=State.AdditiveDueSeconds)Enter(EProcessAutoStage::OpenReceiver,S,Now);
  }break;
 case EProcessAutoStage::BeginB:Go(EProcessCommand::BeginB,TEXT("plant"),0,EProcessAutoStage::SupplyPressure);break;
 case EProcessAutoStage::CombineAB:Go(EProcessCommand::CombineAB,TEXT("plant"),0,EProcessAutoStage::OpenReceiver);break;
 case EProcessAutoStage::OpenReceiver:
  if(State.AdditiveEnabled&&State.IsAdditive()&&State.AdditiveScheduled&&S.PhysicalSeconds<State.AdditiveDueSeconds){State.Reason=TEXT("예약 해석 시각 전 · 수취 출구 닫힘 유지");break;}
  if(!S.AppliedPressureValid||!S.SuppliersVented()||S.AppliedGaugePressurePa!=FVector::ZeroVector||S.AppliedHopperOpen||!PurgeOff(S))
  {Fail(TEXT("혼합 투입 전 대기압·칭량 닫힘·공기 차단 미확인"),M,Now,Send);break;}
  Go(EProcessCommand::Valve,TEXT("receiver"),1,EProcessAutoStage::FeedingMixer);break;
 case EProcessAutoStage::FeedingMixer:{
  const auto& F=S.MixerFeed;State.Reason=F.Reason;
  if(!S.AppliedReceiverOpen||!S.RequestedReceiverOpen||S.AppliedHopperOpen||!PurgeOff(S)||!S.SuppliersVented()||S.AppliedGaugePressurePa!=FVector::ZeroVector)
  {Fail(TEXT("혼합 투입 유로·압력 적용 상태 변경"),M,Now,Send);break;}
  if((State.BatchMode!=EProcessBatchMode::SequentialAB||(S.SequentialBatch.Combined&&S.SequentialBatch.RegularsInMixer()))&&F.State==EPlantTransferState::Complete&&F.ReceiverKg<=S.Transfer.Criteria.WeighingResidualKg&&F.ConnectorKg<=S.Transfer.Criteria.PipeResidualKg&&
     FMath::Abs(F.MixerKg-State.BatchKg)<=S.Transfer.Criteria.MassErrorKg&&F.AdditionalInflowCount==0&&F.StableSeconds>=S.Transfer.Criteria.StableSimulationSeconds)
   Enter(EProcessAutoStage::CloseReceiver,S,Now);
  else if(!WatchProgress(S,Settings.MixerFeedSimulationSeconds,Why))Fail(Why,M,Now,Send);
  break;}
 case EProcessAutoStage::CloseReceiver:{
  EProcessAutoStage Next=EProcessAutoStage::SetMixerRPM;
  if(State.AdditiveEnabled){
   if(!State.IsAdditive()&&(!State.AdditiveScheduled||State.AdditiveDelaySeconds==0))Next=EProcessAutoStage::BeginAdditive;
   if(State.IsAdditive()&&State.MixerAppliedSimulationSeconds>=0)Next=EProcessAutoStage::Mixing;
  }
  if(Go(EProcessCommand::Valve,TEXT("receiver"),0,Next)){
   if(S.MixerFeed.CloseRevision!=State.ReceiverCloseRevision||State.ReceiverClosedSimulationSeconds<0||S.AppliedReceiverOpen){
    Fail(TEXT("수취 출구 닫힘 작업자 응답 불일치 · 혼합 시작 금지"),M,Now,Send);break;}
   if(State.AdditiveEnabled&&State.IsAdditive()){State.AdditiveDelivered=true;State.AdditiveClosedSeconds=State.ReceiverClosedSimulationSeconds;State.AdditiveStatus=TEXT("첨가제 전량 투입·수취 출구 실제 닫힘 확인");}
  }break;}

 case EProcessAutoStage::SetMixerRPM:Go(EProcessCommand::MixerRPM,TEXT("mixer"),State.TargetMixerRPM,EProcessAutoStage::StartMixer);break;
 case EProcessAutoStage::StartMixer:{
  const bool PreparedScheduled=State.AdditiveEnabled&&State.IsAdditive()&&State.AdditiveScheduled;
  const auto& StartFeed=PreparedScheduled?S.InitialBatchMixerFeed:S.MixerFeed;
  if(S.AppliedReceiverOpen||S.RequestedReceiverOpen||S.AppliedHopperOpen||
    (PreparedScheduled?(!S.SequentialBatch.AdditiveReady||B.ReceiverCount!=State.BatchCount):B.MixerCount!=State.BatchCount)||StartFeed.State!=EPlantTransferState::Complete)
  {Fail(TEXT("혼합 시작 전 수취 닫힘·전량 투입 재확인 실패"),M,Now,Send);break;}
  const auto Next=State.AdditiveEnabled&&State.AdditiveScheduled?(State.IsAdditive()?EProcessAutoStage::WaitAdditive:EProcessAutoStage::BeginAdditive):EProcessAutoStage::Mixing;
  if(Go(EProcessCommand::MixerEnabled,TEXT("mixer"),1,Next)){
   if(!S.AppliedRPMValid||!S.MixerTargetApplied(State.TargetMixerRPM)||StartFeed.MixerAppliedRevision!=State.MixerRequestRevision||
      StartFeed.CloseRevision>=StartFeed.MixerStartRevision){Fail(TEXT("혼합 계산 RPM·닫힘 선행 작업자 확인 실패"),M,Now,Send);break;}
   ConfirmMixStart();
   State.MixerAppliedSimulationSeconds=StartFeed.MixerAppliedSimulationSeconds;
   if(!FMath::IsFinite(State.MixerAppliedSimulationSeconds)||State.MixerAppliedSimulationSeconds<0||State.MixerAppliedSimulationSeconds>S.PhysicalSeconds){Fail(TEXT("혼합 시작 적용 시각을 확인할 수 없습니다."),M,Now,Send);break;}
   State.MixingDeadlineSimulationSeconds=State.MixerAppliedSimulationSeconds+State.MixingDurationSeconds;
   if(State.AdditiveEnabled&&State.AdditiveScheduled)State.AdditiveDueSeconds=State.MixerAppliedSimulationSeconds+State.AdditiveDelaySeconds;
   if(!FMath::IsFinite(State.MixingDeadlineSimulationSeconds)){Fail(TEXT("혼합 종료 해석 시각 계산 범위 초과"),M,Now,Send);break;}
   State.MixingElapsedSeconds=MixElapsed(S);
   State.MixingRemainingSeconds=FMath::Max(0.,State.MixingDurationSeconds-State.MixingElapsedSeconds);
   State.Reason=TEXT("작업자 목표 RPM 실제 적용 시각부터 혼합 시간 관찰");
  }break;}
 case EProcessAutoStage::Mixing:
  if(!S.AppliedRPMValid||!S.WorkerMixerEnabled||!S.RequestedMixerEnabled||
     !MixSpeedMatches(M))
   {Fail(TEXT("혼합 중 목표 RPM 이탈 또는 작업자 적용값 미확인"),M,Now,Send);break;}
  if(NewSample&&S.Execution==EPlantExecutionState::Running){
   const double Elapsed=MixElapsed(S);
   if(!FMath::IsFinite(Elapsed)||Elapsed<State.MixingElapsedSeconds){Fail(TEXT("혼합 해석 시간 연속성 오류"),M,Now,Send);break;}
   State.MixingElapsedSeconds=Elapsed;State.MixingRemainingSeconds=FMath::Max(0.,State.MixingDurationSeconds-Elapsed);
   if(Elapsed>=State.MixingDurationSeconds){
    State.MixingTargetReachedSimulationSeconds=S.PhysicalSeconds;
    Enter(EProcessAutoStage::StopMixer,S,Now);
    Go(EProcessCommand::MixerEnabled,TEXT("mixer"),0,EProcessAutoStage::Complete);break;
   }
  }
  State.Reason=FString::Printf(TEXT("목표 %.3f초 · 경과 %.3f초 · 남음 %.3f초 · 계산 적용 %.3f RPM"),State.MixingDurationSeconds,State.MixingElapsedSeconds,State.MixingRemainingSeconds,S.AppliedRPM);
  break;
 case EProcessAutoStage::StopMixer:
  if(NewSample&&S.Execution==EPlantExecutionState::Running&&S.AppliedRPMValid&&S.AppliedRPM>0){
   if(!MixSpeedMatches(M)){Fail(TEXT("혼합 정지 확인 중 목표 RPM 이탈"),M,Now,Send);break;}
   State.MixingElapsedSeconds=MixElapsed(S);
  }
  if(Go(EProcessCommand::MixerEnabled,TEXT("mixer"),0,EProcessAutoStage::Complete)){
   if(State.MixerStopRequestRevision<=0||S.AppliedRPMRevision!=State.MixerStopRequestRevision||!S.AppliedRPMValid||S.AppliedRPM!=0||
      S.RequestedMixerEnabled||S.WorkerMixerEnabled||!FMath::IsFinite(S.AppliedRPMSimulationSeconds)||
      S.AppliedRPMSimulationSeconds<State.MixerStopRequestedSimulationSeconds||S.AppliedRPMSimulationSeconds>S.PhysicalSeconds)
    {Fail(TEXT("이번 혼합 정지 요청의 작업자 RPM0 적용 시각·응답 불일치"),M,Now,Send);break;}
   State.MixerStoppedSimulationSeconds=S.AppliedRPMSimulationSeconds;
   State.MixingElapsedSeconds=(S.Recovery.Supported?S.Recovery.AutomaticMixSeconds:State.MixerStoppedSimulationSeconds-State.MixerAppliedSimulationSeconds);
   State.MixingRemainingSeconds=0;State.MixingStopOverrunSeconds=S.Recovery.Supported?FMath::Max(0.,State.MixingElapsedSeconds-State.MixingDurationSeconds):State.MixerStoppedSimulationSeconds-State.MixingDeadlineSimulationSeconds;
   State.FinishedSimulationSeconds=S.PhysicalSeconds;State.Active=false;M.AutomationActive=false;
   State.Reason=TEXT("설정 혼합 시간 경과·이번 정지 요청의 계산 적용 RPM0 확인");
   UE_LOG(LogTemp,Display,TEXT("AUTO_A_BATCH_COMPLETE epoch=%lld start=%.9f target=%.9f reached=%.9f requested=%.9f stopped=%.9f elapsed=%.9f overrun=%.9f revision=%lld"),State.Epoch,State.MixerAppliedSimulationSeconds,State.MixingDurationSeconds,State.MixingTargetReachedSimulationSeconds,State.MixerStopRequestedSimulationSeconds,State.MixerStoppedSimulationSeconds,State.MixingElapsedSeconds,State.MixingStopOverrunSeconds,State.MixerStopRequestRevision);
  }break;
 default:break;
 }
}
