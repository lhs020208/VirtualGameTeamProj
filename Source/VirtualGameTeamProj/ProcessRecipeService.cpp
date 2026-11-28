#include "ProcessControlService.h"
#include "PowderPlantTest.h"

FProcessRecipe UProcessControlService::GetAppliedRecipe() const
{
 FProcessRecipe R=RecipeMetadata;R.Name=R.Name.IsEmpty()?TEXT("개별 설정"):R.Name;
 const auto& S=AutoSettings;R.Entries=S.Entries;R.SupplierFeedPa=S.SupplierFeedPa;
 if(!S.Entries.IsEmpty()){
  R.Mode=EProcessBatchMode::HopperSequence;R.MixingSeconds=S.MixingDurationSeconds;R.TargetRPM=S.TargetMixerRPM;
  R.Scheduled=S.AdditiveScheduled;R.DelaySeconds=S.AdditiveDelaySeconds;R.AdditiveEnabled=S.AdditiveEnabled;
  R.FeedPressurePa=S.FeedPressurePa;R.TransferPressurePa=S.TransferPressurePa;return R;
 }
 R.Mode=S.BatchMode;R.TargetAKg=R.ToleranceAKg=R.TargetBKg=R.ToleranceBKg=0;
 R.AdditiveEnabled=R.Scheduled=false;R.TargetCKg=R.ToleranceCKg=R.DelaySeconds=0;
 if(R.Mode!=EProcessBatchMode::Full){R.TargetAKg=S.TargetMassKg;R.ToleranceAKg=S.WeighingToleranceKg;}
 if(R.Mode==EProcessBatchMode::SequentialAB){R.TargetBKg=S.TargetMassBKg;R.ToleranceBKg=S.WeighingToleranceBKg;R.AdditiveEnabled=S.AdditiveEnabled;}
 if(R.AdditiveEnabled){R.TargetCKg=S.AdditiveTargetKg;R.ToleranceCKg=S.AdditiveToleranceKg;R.Scheduled=S.AdditiveScheduled;R.DelaySeconds=R.Scheduled?S.AdditiveDelaySeconds:0;}
 R.FeedPressurePa=S.FeedPressurePa;R.TransferPressurePa=S.TransferPressurePa;
 R.TargetRPM=S.TargetMixerRPM!=0?S.TargetMixerRPM:State.Snapshot.RequestedRPM;R.MixingSeconds=S.MixingDurationSeconds;return R;
}
bool UProcessControlService::ApplyRecipe(const FProcessRecipe& R,FString& Reason)
{
 check(IsInGameThread());
 if(WebOwnsControl()&&!WebDispatch){Reason=LocalOperatorReason();return false;}
 if(AutoRun.State.Active||State.AutomationActive||State.IsStopping()){
  Reason=TEXT("자동 배치 또는 전체 정지 처리 중에는 레시피를 적용할 수 없습니다. 이번 실행 설정은 유지됩니다.");return false;
 }
 Reason=ProcessRecipe::Validate(R);if(!Reason.IsEmpty())return false;
 // Validate everything before assigning. Never Tick, Request, access the worker or issue RPM here.
 auto Next=AutoSettings;Next.RequiredDiametersMm=R.DiameterMm;Next.BatchMode=R.Mode;Next.TargetMassKg=R.TargetAKg;Next.WeighingToleranceKg=R.ToleranceAKg;
 Next.TargetMassBKg=R.TargetBKg;Next.WeighingToleranceBKg=R.ToleranceBKg;Next.AdditiveEnabled=R.AdditiveEnabled;
 Next.AdditiveSupplier=2;Next.AdditiveTargetKg=R.TargetCKg;Next.AdditiveToleranceKg=R.ToleranceCKg;
 Next.AdditiveScheduled=R.Scheduled;Next.AdditiveDelaySeconds=R.DelaySeconds;Next.TargetMixerRPM=R.TargetRPM;Next.MixingDurationSeconds=R.MixingSeconds;
 Next.FeedPressurePa=R.FeedPressurePa;Next.TransferPressurePa=R.TransferPressurePa;
 Next.Entries=R.Entries;Next.SupplierFeedPa=R.SupplierFeedPa;
 if(R.Mode==EProcessBatchMode::HopperSequence){
  Next.BatchMode=EProcessBatchMode::SequentialAB;
  for(const auto& E:R.Entries)if(E.Enabled&&E.Additive){
   Next.AdditiveSupplier=E.Supplier;Next.AdditiveTargetKg=E.TargetKg;Next.AdditiveToleranceKg=E.ToleranceKg;
  }
 }
 if(auto* Actor=Source.Get())Actor->NextChargeDiameterMm=R.DiameterMm;
 AutoSettings=Next;AutoRun.Settings=Next;RecipeMetadata=R;++RecipeRevision;
 // Inventory and representable particle mass remain checked by the existing CanStart path.
 return true;
}

FString UProcessControlService::NewChargeReason() const
{
 if(!Source.IsValid()||!State.Connected)return TEXT("해석 Actor 연결 대기");
 if(AutoRun.State.Active||AutoRun.State.RecoveryPending||State.AutomationActive||State.IsStopping())return TEXT("실행 또는 복구할 배치가 있습니다.");
 if(State.Snapshot.Execution!=EPlantExecutionState::Paused||State.StopStage!=EProcessStopStage::Complete)return TEXT("전체 정지가 확인된 뒤 새 장입이 가능합니다.");
 if(RecipeRevision<=0)return TEXT("먼저 입자 설정을 포함한 레시피를 적용하세요.");
 return FString();
}
bool UProcessControlService::LoadNewCharge(FString& Reason)
{
 if(WebOwnsControl()&&!WebDispatch){Reason=LocalOperatorReason();return false;}
 Reason=NewChargeReason();if(!Reason.IsEmpty())return false;
 auto* Actor=Source.Get();Actor->NextChargeDiameterMm=RecipeMetadata.DiameterMm;
 Actor->ResetSimulation();Reason=Actor->Error;return Reason.IsEmpty();
}
