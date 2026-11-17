#include "ProcessControlService.h"

FProcessRecipe UProcessControlService::GetAppliedRecipe() const
{
 FProcessRecipe R=RecipeMetadata;R.Name=R.Name.IsEmpty()?TEXT("개별 설정"):R.Name;
 const auto& S=AutoSettings;R.Mode=S.BatchMode;R.TargetAKg=R.ToleranceAKg=R.TargetBKg=R.ToleranceBKg=0;
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
 if(AutoRun.State.Active||State.AutomationActive||State.IsStopping()){
  Reason=TEXT("자동 배치 또는 전체 정지 처리 중에는 레시피를 적용할 수 없습니다. 이번 실행 설정은 유지됩니다.");return false;
 }
 Reason=ProcessRecipe::Validate(R);if(!Reason.IsEmpty())return false;
 // Validate everything before assigning. Never Tick, Request, access the worker or issue RPM here.
 auto Next=AutoSettings;Next.BatchMode=R.Mode;Next.TargetMassKg=R.TargetAKg;Next.WeighingToleranceKg=R.ToleranceAKg;
 Next.TargetMassBKg=R.TargetBKg;Next.WeighingToleranceBKg=R.ToleranceBKg;Next.AdditiveEnabled=R.AdditiveEnabled;
 Next.AdditiveSupplier=2;Next.AdditiveTargetKg=R.TargetCKg;Next.AdditiveToleranceKg=R.ToleranceCKg;
 Next.AdditiveScheduled=R.Scheduled;Next.AdditiveDelaySeconds=R.DelaySeconds;Next.TargetMixerRPM=R.TargetRPM;Next.MixingDurationSeconds=R.MixingSeconds;
 Next.FeedPressurePa=R.FeedPressurePa;Next.TransferPressurePa=R.TransferPressurePa;
 AutoSettings=Next;AutoRun.Settings=Next;RecipeMetadata=R;++RecipeRevision;
 // Inventory and representable particle mass remain checked by the existing CanStart path.
 return true;
}
