#include "ProcessAutoRun.h"

namespace {
bool Stopped(const FPlantSimulationSnapshot& S){
 if(S.Execution!=EPlantExecutionState::Paused||S.RequestedRunning||S.AppliedSupplierMask||S.AppliedHopperOpen||S.AppliedReceiverOpen||S.RequestedHopperOpen||S.RequestedReceiverOpen||
    !S.AppliedRPMValid||S.AppliedRPM!=0||S.RequestedMixerEnabled||S.WorkerMixerEnabled||!S.AppliedPressureValid||!S.SuppliersVented()||S.AppliedGaugePressurePa!=FVector::ZeroVector||S.RequestedGaugePressurePa!=FVector::ZeroVector)return false;
 for(bool Open:S.RequestedSuppliers)if(Open)return false;
 for(int32 I=0;I<6;++I){const auto& P=PlantPurgeFor(S,I);if(P.Supported&&(!P.Valid||P.RequestedEnabled||P.AppliedEnabled||P.RequestedGaugePa||P.AppliedGaugePa||P.FlowM3s))return false;}
 return true;
}
}
void FProcessAutoRun::ObserveRecovery(const FProcessControlMachine& M)
{
 if(!State.RecoveryPending)return;
 const auto& S=M.Snapshot;
 if(State.Epoch!=S.Epoch||State.Connection!=M.Connection||!M.Connected){State.RecoveryPending=false;State.RecoveryReason=TEXT("초기화·연결 세대 변경 · 이전 배치 재개 불가");return;}
 if(Stopped(S)&&M.StopStage==EProcessStopStage::Complete&&State.StopConfirmedSeconds<0){State.StopConfirmedSeconds=S.PhysicalSeconds;State.StopInventory=S.Recovery;}
 const auto& F=S.SequentialBatch.AdditiveScheduled?S.InitialBatchMixerFeed:S.MixerFeed;
 if(State.MixerAppliedSimulationSeconds<0&&F.MixerAppliedRevision>0){
  State.MixerAppliedSimulationSeconds=F.MixerAppliedSimulationSeconds;
  State.MixingDeadlineSimulationSeconds=F.MixerAppliedSimulationSeconds+State.MixingDurationSeconds;
  if(State.AdditiveEnabled&&State.AdditiveScheduled)State.AdditiveDueSeconds=F.MixerAppliedSimulationSeconds+State.AdditiveDelaySeconds;
 }
 if(S.Recovery.Supported)State.MixingElapsedSeconds=S.Recovery.AutomaticMixSeconds;
 State.MixingRemainingSeconds=FMath::Max(0.,State.MixingDurationSeconds-State.MixingElapsedSeconds);
 State.RecoveryNext=RecoveryPlan(S);
 if(!S.Recovery.ForeignIds.IsEmpty())State.RecoveryReason=FString::Printf(TEXT("배치 재개 차단 · 오원료 ID %d / 공급원 %d / %s (유입 기록 유지)"),S.Recovery.ForeignIds[0],S.Recovery.ForeignSuppliers[0],*S.Recovery.ForeignRegions[0]);
}
FString FProcessAutoRun::CanRecover(const FProcessControlMachine& M,double Now) const
{
 const auto& S=M.Snapshot;const auto& B=S.SupplyABatch;const auto& R=S.Recovery;
 if(State.Active)return TEXT("자동 실행 중 · 중복 재개는 발행하지 않습니다.");
 if(!State.RecoveryPending||State.RecoveryAbandoned||!State.RunId.IsValid())return TEXT("재개 가능한 중단 배치가 없습니다.");
 if(!M.Connected||S.Epoch!=State.Epoch||M.Connection!=State.Connection)return TEXT("초기화·연결 변경으로 이전 배치는 재개할 수 없습니다.");
 if(!R.ForeignIds.IsEmpty())return FString::Printf(TEXT("배치 재개 차단 · ID %d, 공급원 %d, 유입 구역 %s · 배치 중단/초기화 필요"),R.ForeignIds[0],R.ForeignSuppliers[0],*R.ForeignRegions[0]);
 if(S.Validity!=EPlantValueValidity::Valid||Now<S.PublishedWallSeconds||Now-S.PublishedWallSeconds>2.||!R.Supported||!R.Valid||!B.Valid||!S.Error.IsEmpty())return TEXT("최신 작업자 좌표·원료 출처·배치 이력 미확인");
 if(!Stopped(S)||M.IsStopping()||M.StopStage!=EProcessStopStage::Complete)return TEXT("수동 조치 후 전체 정지를 완료하고 상태를 다시 확인하세요.");
 for(const auto& P:M.History)if(P.Value.IsPending())return TEXT("남아 있는 수동 요청의 적용 확인 중");
 if(B.CaptureRevision<=0||B.ForeignCount||B.UnknownCount||!R.Materials.IsValidIndex(S.SequentialBatch.ActiveSupplier))return TEXT("배치 출처·입자 누락 또는 미분류 위치 확인 필요");
 for(const auto& Material:R.Materials){
  if(Material.Unknown)return TEXT("원료별 현재 위치 미확인 · 재개 보류");
  if(State.StopInventory.Materials.IsValidIndex(Material.Supplier)){
   const auto& AtStop=State.StopInventory.Materials[Material.Supplier];
   if(AtStop.Supplier!=Material.Supplier||AtStop.SelectedIds!=Material.SelectedIds)return TEXT("정지 확인 시점과 현재 확정 계량분의 공급원 또는 입자 목록 불일치");
  }
 }
 if(B.DoseFinalized&&(B.WeighingCount||B.PipeCount)&&S.Transfer.Id>0&&S.Transfer.State==EPlantTransferState::Indeterminate)return TEXT("기존 수취 이송 기록 판정 불가 · 재개 보류");
 if((B.MixerCount||B.MixerConnectorCount)&&(S.MixerFeed.State==EPlantTransferState::Indeterminate||S.MixerFeed.OpeningRevision<=0))return TEXT("혼합 투입의 승인·기준량 기록 미확인 · 재개 보류");
 if(B.DoseFinalized&&(B.WeighingCount||B.PipeCount)&&S.Transfer.Id>0&&FMath::Abs(S.Transfer.BaselineKg-B.TotalKg)>S.Transfer.Criteria.MassErrorKg)return TEXT("기존 이송 기준량과 확정 계량분 불일치");
 const int32 Supplier=S.SequentialBatch.ActiveSupplier;
 if(!State.UsesSupplier(Supplier)||Supplier<0||Supplier>=6)return TEXT("지원하지 않는 공급원 이력");
 const double Target=State.DoseFor(Supplier,false);
 const double Tol=State.DoseFor(Supplier,true);
 if(!B.DoseFinalized){
  const double Passed=(B.ConnectorCount+B.WeighingCount)*S.ModelParticleMassKg;
  if(State.StopInventory.Materials.IsValidIndex(Supplier)){
   const TSet<int32> CurrentPassed(R.Materials[Supplier].PassedIds);
   for(int32 Id:State.StopInventory.Materials[Supplier].PassedIds)if(!CurrentPassed.Contains(Id))return FString::Printf(TEXT("정지 시 통과 입자 ID %d의 현재 배치 경로 보존 미확인"),Id);
  }
  if(B.PipeCount||B.ReceiverCount||B.MixerCount||B.MixerConnectorCount||B.SourceCount+B.ConnectorCount+B.WeighingCount!=B.TotalCount)return TEXT("계량 확정 전 예상하지 않은 배출/입자 누락 · 재개 보류");
  if(State.BatchMode!=EProcessBatchMode::Full&&Passed>Target+Tol)return TEXT("이미 공급한 양이 계량 상한을 초과했습니다. 재개할 수 없습니다.");
 }else if(B.SourceCount||B.ConnectorCount||B.WeighingCount+B.PipeCount+B.ReceiverCount+B.MixerConnectorCount+B.MixerCount!=B.TotalCount||FMath::Abs(B.TotalCount*S.ModelParticleMassKg-B.TotalKg)>S.Transfer.Criteria.MassErrorKg)return TEXT("확정 입자 목록·질량과 현재 위치 불일치");
 if(State.AdditiveEnabled&&State.AdditiveScheduled&&State.MixerAppliedSimulationSeconds>=0&&State.AdditiveOpenRevision==0&&
   (S.PhysicalSeconds>=State.AdditiveDueSeconds||R.AutomaticMixSeconds>=State.AdditiveDelaySeconds))return TEXT("미실행 예약 시점 경과 · 늦은 자동 투입 금지");
 const bool Delivered=S.SequentialBatch.AdditiveActive&&B.TotalCount>0&&B.MixerCount==B.TotalCount&&S.MixerFeed.State==EPlantTransferState::Complete&&S.MixerFeed.CloseRevision>S.MixerFeed.OpeningRevision&&S.MixerFeed.OpeningRevision>0;
 if(State.MixerAppliedSimulationSeconds>=0&&R.AutomaticMixSeconds>=State.MixingDurationSeconds&&State.AdditiveEnabled&&!State.AdditiveDelivered&&!Delivered)return TEXT("혼합 시간 종료·첨가제 미완료 · 자동 연장 금지");
 return FString();
}
bool FProcessAutoRun::Recover(FProcessControlMachine& M,double Now)
{
 if(State.Active)return false;
 ObserveRecovery(M);const auto Why=CanRecover(M,Now);if(!Why.IsEmpty()){State.RecoveryReason=Why;return false;}
 const auto& S=M.Snapshot;const auto& B=S.SupplyABatch;const auto& Q=S.SequentialBatch;
 State.BatchRevision=B.CaptureRevision;State.BatchCount=B.TotalCount;State.BatchKg=B.TotalKg;State.ActiveSupplier=Q.ActiveSupplier;
 State.TargetMassKg=State.DoseFor(State.ActiveSupplier,false);
 State.WeighingToleranceKg=State.DoseFor(State.ActiveSupplier,true);
 if(!State.Entries.IsEmpty()){
  State.FinalBySupplierKg.Init(-1,6);
  for(int32 I=0;I<Q.Materials.Num()&&I<6;++I)if(Q.Materials[I].FinalSimulationSeconds>=0)State.FinalBySupplierKg[I]=Q.Materials[I].FinalKg;
 }
 State.FinalAKg=Q.A.FinalKg;State.FinalBKg=Q.B.FinalKg;State.FinalAdditiveKg=Q.Additive.FinalKg;
 State.DoseFinalized=B.DoseFinalized;State.DoseFinalizeRevision=B.DoseFinalizeRevision;State.FinalDoseKg=B.DoseFinalKg;State.FinalDoseSimulationSeconds=B.DoseFinalSimulationSeconds;
 const double Passed=(B.ConnectorCount+B.WeighingCount)*S.ModelParticleMassKg;
 State.RemainingSupplyKg=B.DoseFinalized?0:FMath::Max(0.,(State.BatchMode==EProcessBatchMode::Full?B.InitialKg:State.TargetMassKg)-Passed);
 if(Q.AdditiveActive&&B.TotalCount>0&&B.MixerCount==B.TotalCount&&S.MixerFeed.State==EPlantTransferState::Complete&&S.MixerFeed.CloseRevision>S.MixerFeed.OpeningRevision&&S.MixerFeed.OpeningRevision>0){State.AdditiveDelivered=true;State.AdditiveClosedSeconds=S.MixerFeed.ClosedSimulationSeconds;}
 State.RecoveryNext=RecoveryPlan(S);
 State.DownstreamStarted=State.RecoveryNext==EProcessAutoStage::OpenReceiver||State.RecoveryNext==EProcessAutoStage::CloseReceiver||State.RecoveryNext==EProcessAutoStage::Mixing;
 // Existing transfer IDs and cohorts are retained. Only new control requests are issued.
 State.TransferId=S.Transfer.Id;State.RemainingKg=S.Transfer.RemainingKg;State.TransitKg=S.Transfer.InTransitKg;State.ArrivedKg=S.Transfer.ArrivedKg;
 State.RecoveryPending=false;State.Active=true;State.RecoverySeconds=S.PhysicalSeconds;++State.RecoveryCount;M.AutomationActive=true;
 State.RecoveryReason=TEXT("현재 작업자 위치로 남은 단계 계산 · 저장된 명령 복원 없음");LastClockSequence=-1;
 Enter(EProcessAutoStage::RecoveryAdmit,S,Now);return true;
}
bool FProcessAutoRun::TickRecovery(FProcessControlMachine& M,double Now,const FProcessAutoCommand& Send)
{
 if(State.Stage<EProcessAutoStage::RecoveryAdmit||State.Stage>EProcessAutoStage::RecoveryCloseOutlet)return false;
 const auto& S=M.Snapshot;
 if(Now-EnteredWall>Settings.StageWallSeconds){Fail(TEXT("배치 재개 작업자 응답 시간 초과"),M,Now,Send);return true;}
 if(!S.Recovery.ForeignIds.IsEmpty()||S.SupplyABatch.ForeignCount||S.SupplyABatch.UnknownCount){Fail(TEXT("재개 준비 중 원료 이력/현재 위치 변경 · 전체 정지"),M,Now,Send);return true;}
 const auto Go=[&](EProcessCommand C,const TCHAR* Id,double V,EProcessAutoStage N){return Command(C,Id,V,N,M,Now,Send);};
 switch(State.Stage){
 case EProcessAutoStage::RecoveryAdmit:Go(EProcessCommand::RecoverBatch,TEXT("plant"),0,EProcessAutoStage::RecoveryRun);break;
 case EProcessAutoStage::RecoveryRun:Go(EProcessCommand::Resume,TEXT("plant"),1,State.MixerAppliedSimulationSeconds>=0&&State.MixingElapsedSeconds<State.MixingDurationSeconds?EProcessAutoStage::RecoveryRPM:EProcessAutoStage::RecoveryRoute);break;
 case EProcessAutoStage::RecoveryRPM:Go(EProcessCommand::MixerRPM,TEXT("mixer"),State.TargetMixerRPM,EProcessAutoStage::RecoveryMix);break;
 case EProcessAutoStage::RecoveryMix:
  if(Go(EProcessCommand::MixerEnabled,TEXT("mixer"),1,EProcessAutoStage::RecoveryRoute)){
   const auto& F=S.SequentialBatch.AdditiveScheduled?S.InitialBatchMixerFeed:S.MixerFeed;
   if(!S.AppliedRPMValid||S.AppliedRPM!=double(float(State.TargetMixerRPM))||F.MixerAppliedRevision!=State.MixerRequestRevision){Fail(TEXT("재개 혼합의 새 요청 RPM 적용 미확인"),M,Now,Send);break;}
   ConfirmMixStart();
   State.MixingElapsedSeconds=MixElapsed(S);State.MixingDeadlineSimulationSeconds=S.PhysicalSeconds+State.MixingDurationSeconds-State.MixingElapsedSeconds;
   if(State.AdditiveEnabled&&State.AdditiveScheduled&&State.AdditiveOpenRevision==0)State.AdditiveDueSeconds=S.PhysicalSeconds+State.AdditiveDelaySeconds-State.MixingElapsedSeconds;
  }break;
 case EProcessAutoStage::RecoveryRoute:
  if(State.MixingElapsedSeconds>=State.MixingDurationSeconds&&State.MixerAppliedSimulationSeconds>=0)Enter(EProcessAutoStage::StopMixer,S,Now);
  else Enter(State.RecoveryNext,S,Now);break;
 case EProcessAutoStage::RecoveryCloseOutlet:Go(EProcessCommand::Valve,TEXT("weigher"),0,EProcessAutoStage::Transferring);break;
 case EProcessAutoStage::RecoveryClearPressure:Go(EProcessCommand::Pressure,TEXT("weigher"),-500,EProcessAutoStage::PurgePressure);break;
 default:break;
 }
 return true;
}
EProcessAutoStage FProcessAutoRun::RecoveryPlan(const FPlantSimulationSnapshot& S) const
{
 const auto& B=S.SupplyABatch;const auto& Q=S.SequentialBatch;
 const double Target=State.DoseFor(Q.ActiveSupplier,false);
 const double Tol=State.DoseFor(Q.ActiveSupplier,true);
 if(!B.DoseFinalized){
  const double Passed=(B.WeighingCount+B.ConnectorCount)*S.ModelParticleMassKg;
  const bool Enough=State.BatchMode==EProcessBatchMode::Full?B.SourceCount==0:Passed>=Target-Tol;
  return Enough?(B.ConnectorCount?EProcessAutoStage::RecoveryClearPressure:EProcessAutoStage::WeighingStable):EProcessAutoStage::SupplyPressure;
 }
 if(B.WeighingCount||B.PipeCount)return EProcessAutoStage::TransferPressure;
 if(!Q.Combined&&State.BatchMode==EProcessBatchMode::SequentialAB)return EProcessAutoStage::Transferring;
 if(B.MixerCount==B.TotalCount)return State.AdditiveDelivered?EProcessAutoStage::Mixing:EProcessAutoStage::CloseReceiver;
 if(Q.AdditiveActive&&S.MixerFeed.OpeningRevision==0)return EProcessAutoStage::Transferring;
 return EProcessAutoStage::OpenReceiver;
}
