#pragma once
#include "CoreMinimal.h"
#include "PlantBatchRecoveryTypes.h"
#include "ProcessAutoTypes.generated.h"

UENUM(BlueprintType)
enum class EProcessAutoStage : uint8
{
 Idle, InitialStop, CaptureBatch, Resume, SupplyPressure, FeedPressure, OpenA, Feeding,
 CloseA, SupplyZero, PurgePressure, PurgeStart, Clearing, PurgeStop, WeighingStable,
 TransferPressure, ReceiverPressure, OpenWeigher, Transferring, CloseWeigher,
 Complete, Stopping, Cancelled, Failed, VentWeigher, VentReceiver, OpenReceiver, FeedingMixer, CloseReceiver, SetMixerRPM, StartMixer, Mixing, StopMixer, FinalizeDose, BeginB, CombineAB, BeginAdditive, WaitAdditive, RecoveryAdmit, RecoveryRun, RecoveryRPM, RecoveryMix, RecoveryRoute, RecoveryClearPressure, RecoveryCloseOutlet, FeedReceiverBaseline, PurgeWeigherBaseline, PurgeReceiverBaseline
};
UENUM(BlueprintType)
enum class EProcessBatchMode : uint8 { Full, TargetMass, SequentialAB, HopperSequence };
USTRUCT(BlueprintType)
struct FProcessFeedEntry
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere) int32 Supplier=INDEX_NONE;
 UPROPERTY(EditAnywhere) bool Enabled=true;
 UPROPERTY(EditAnywhere) bool Additive=false;
 UPROPERTY(EditAnywhere) FString DisplayName;
 UPROPERTY(EditAnywhere) double TargetKg=0;
 UPROPERTY(EditAnywhere) double ToleranceKg=0;
};
USTRUCT(BlueprintType)
struct FProcessAutoSettings
{
 GENERATED_BODY()
 UPROPERTY() TArray<double> RequiredDiametersMm;
 UPROPERTY(EditAnywhere) TArray<FProcessFeedEntry> Entries;
 // Trial observation limits, not changes to physics or completion tolerances.
 UPROPERTY(EditAnywhere) TArray<double> SupplierFeedPa;
 UPROPERTY(EditAnywhere) FVector FeedPressurePa=FVector(500,-500,0);
 UPROPERTY(EditAnywhere) FVector TransferPressurePa=FVector(0,500,-1000);
 UPROPERTY(EditAnywhere) double StageWallSeconds=1800;
 UPROPERTY(EditAnywhere) double FeedSimulationSeconds=3;
 UPROPERTY(EditAnywhere) double PurgeSimulationSeconds=3;
 UPROPERTY(EditAnywhere) double TransferSimulationSeconds=3;
 UPROPERTY(EditAnywhere) double StagnationSimulationSeconds=.7;
 UPROPERTY(EditAnywhere) double MixerFeedSimulationSeconds=3;
 UPROPERTY(EditAnywhere) bool AdditiveEnabled=false;
 UPROPERTY(EditAnywhere) int32 AdditiveSupplier=2;
 UPROPERTY(EditAnywhere) bool AdditiveScheduled=false;
 UPROPERTY(EditAnywhere) double AdditiveTargetKg=0;
 UPROPERTY(EditAnywhere) double AdditiveToleranceKg=0;
 UPROPERTY(EditAnywhere) double AdditiveDelaySeconds=0;
 // Configuration-only RPM. Zero preserves the legacy individual RPM setting path.
 UPROPERTY(EditAnywhere) double TargetMixerRPM=0;
 // Unset until explicitly configured; no trial duration is an operating default.
 UPROPERTY(EditAnywhere) double MixingDurationSeconds=0;
 UPROPERTY(EditAnywhere) EProcessBatchMode BatchMode=EProcessBatchMode::Full;
 UPROPERTY(EditAnywhere) double TargetMassKg=0;
 UPROPERTY(EditAnywhere) double TargetMassBKg=0;
 UPROPERTY(EditAnywhere) double WeighingToleranceBKg=0;
 UPROPERTY(EditAnywhere) double WeighingToleranceKg=0; // Explicit UI input; no trial defaults.

};
USTRUCT(BlueprintType)
struct FProcessAutoSnapshot
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) TArray<FProcessFeedEntry> Entries;
 UPROPERTY(BlueprintReadOnly) TArray<double> FinalBySupplierKg;
 const FProcessFeedEntry* EntryFor(int32 Supplier) const
 {
  return Entries.FindByPredicate([Supplier](const FProcessFeedEntry& E){return E.Enabled&&E.Supplier==Supplier;});
 }
 bool UsesSupplier(int32 Supplier) const
 {
  if(!Entries.IsEmpty())return EntryFor(Supplier)!=nullptr;
  return Supplier==0||(BatchMode==EProcessBatchMode::SequentialAB&&Supplier==1)||(AdditiveEnabled&&Supplier==AdditiveSupplier);
 }
 FString FinalSummary() const
 {
  FString Summary;double Total=0;
  for(const auto& E:Entries){
   if(!E.Enabled)continue;
   if(!FinalBySupplierKg.IsValidIndex(E.Supplier)||FinalBySupplierKg[E.Supplier]<0)return TEXT("최종 계량값 미확인");
   const double Kg=FinalBySupplierKg[E.Supplier];Total+=Kg;
   Summary+=FString::Printf(TEXT("%c · %s  %.6g g\n"),'A'+E.Supplier,*E.DisplayName,Kg*1000);
  }
  return Summary+FString::Printf(TEXT("합계 %.6g g"),Total*1000);
 }
 bool IsAdditive() const {return AdditiveEnabled&&ActiveSupplier==AdditiveSupplier;}
 int32 NextRegular(int32 After) const
 {
  if(Entries.IsEmpty())return After==INDEX_NONE?0:After==0&&BatchMode==EProcessBatchMode::SequentialAB?1:INDEX_NONE;
  bool Found=After==INDEX_NONE;
  for(const auto& Entry:Entries){
   if(!Entry.Enabled||Entry.Additive)continue;
   if(Found)return Entry.Supplier;
   Found=Entry.Supplier==After;
  }
  return INDEX_NONE;
 }
 double DoseFor(int32 Supplier,bool Tolerance) const
 {
  for(const auto& E:Entries)if(E.Enabled&&E.Supplier==Supplier)return Tolerance?E.ToleranceKg:E.TargetKg;
  if(AdditiveEnabled&&Supplier==AdditiveSupplier)return Tolerance?AdditiveToleranceKg:AdditiveTargetKg;
  if(Supplier==1)return Tolerance?LockedToleranceBKg:LockedTargetBKg;
  return Tolerance?LockedToleranceAKg:LockedTargetAKg;
 }
 UPROPERTY(BlueprintReadOnly) FPlantBatchRecoverySnapshot StopInventory;
 UPROPERTY(BlueprintReadOnly) bool RecoveryPending=false;
 UPROPERTY(BlueprintReadOnly) bool RecoveryAbandoned=false;
 UPROPERTY(BlueprintReadOnly) int32 RecoveryCount=0;
 UPROPERTY(BlueprintReadOnly) EProcessAutoStage InterruptedStage=EProcessAutoStage::Idle;
 UPROPERTY(BlueprintReadOnly) EProcessAutoStage RecoveryNext=EProcessAutoStage::Idle;
 UPROPERTY(BlueprintReadOnly) double StopRequestedSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double StopConfirmedSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double RecoverySeconds=-1;
 UPROPERTY(BlueprintReadOnly) double RemainingSupplyKg=0;
 UPROPERTY(BlueprintReadOnly) FString RecoveryReason;

 UPROPERTY(BlueprintReadOnly) EProcessBatchMode BatchMode=EProcessBatchMode::Full;
 UPROPERTY(BlueprintReadOnly) bool AdditiveEnabled=false;
 UPROPERTY(BlueprintReadOnly) int32 AdditiveSupplier=2;
 UPROPERTY(BlueprintReadOnly) bool AdditiveScheduled=false;
 UPROPERTY(BlueprintReadOnly) double AdditiveTargetKg=0;
 UPROPERTY(BlueprintReadOnly) double AdditiveToleranceKg=0;
 UPROPERTY(BlueprintReadOnly) double AdditiveDelaySeconds=0;
 UPROPERTY(BlueprintReadOnly) double FinalAdditiveKg=0;
 UPROPERTY(BlueprintReadOnly) double AdditivePreparationStartedSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double AdditivePreparedSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double AdditiveDueSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double AdditiveOpenRequestedSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double AdditiveOpenAppliedSeconds=-1;
 UPROPERTY(BlueprintReadOnly) int64 AdditiveOpenRevision=0;
 UPROPERTY(BlueprintReadOnly) double AdditiveClosedSeconds=-1;
 UPROPERTY(BlueprintReadOnly) bool AdditiveDelivered=false;
 UPROPERTY(BlueprintReadOnly) FString AdditiveStatus;
 UPROPERTY(BlueprintReadOnly) int32 ActiveSupplier=0;
 UPROPERTY(BlueprintReadOnly) double LockedTargetAKg=0;
 UPROPERTY(BlueprintReadOnly) double LockedToleranceAKg=0;
 UPROPERTY(BlueprintReadOnly) double LockedTargetBKg=0;
 UPROPERTY(BlueprintReadOnly) double LockedToleranceBKg=0;
 UPROPERTY(BlueprintReadOnly) double FinalAKg=0;
 UPROPERTY(BlueprintReadOnly) double FinalBKg=0;
 UPROPERTY(BlueprintReadOnly) double TargetMassKg=0;
 UPROPERTY(BlueprintReadOnly) double WeighingToleranceKg=0;
 UPROPERTY(BlueprintReadOnly) double InitialAvailableKg=0;
 UPROPERTY(BlueprintReadOnly) bool DoseFinalized=false;
 UPROPERTY(BlueprintReadOnly) double FinalDoseKg=0;
 UPROPERTY(BlueprintReadOnly) double FinalDoseSimulationSeconds=-1;
 UPROPERTY(BlueprintReadOnly) int64 DoseFinalizeRevision=0;
 UPROPERTY(BlueprintReadOnly) int64 DoseCloseRequestRevision=0;
 UPROPERTY(BlueprintReadOnly) double CutoffEstimatedKg=0;
 UPROPERTY(BlueprintReadOnly) double CutoffLeadKg=0;
 UPROPERTY(BlueprintReadOnly) double CutoffDrainableKg=0;
 UPROPERTY(BlueprintReadOnly) double CutoffRequestedSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double CutoffWeighingKg=0;
 UPROPERTY(BlueprintReadOnly) double CutoffConnectorKg=0;

 UPROPERTY(BlueprintReadOnly) FString RecipeName;
 UPROPERTY(BlueprintReadOnly) FString RecipeMaterialA;
 UPROPERTY(BlueprintReadOnly) FString RecipeMaterialB;
 UPROPERTY(BlueprintReadOnly) FString RecipeMaterialC;
 UPROPERTY(BlueprintReadOnly) FString LockedRecipeJson;
 UPROPERTY(BlueprintReadOnly) TArray<double> LockedSupplierFeedPa;
 UPROPERTY(BlueprintReadOnly) FGuid PressureStageToken;
 UPROPERTY(BlueprintReadOnly) FString PressureEquipment;
 FVector ExpectedPressurePa=FVector::ZeroVector;
 FVector FeedBaseline() const {
  FVector V=LockedFeedPressurePa;
  if(LockedSupplierFeedPa.IsValidIndex(ActiveSupplier))V.X=LockedSupplierFeedPa[ActiveSupplier];
  return V;
 }
 UPROPERTY(BlueprintReadOnly) FVector LockedFeedPressurePa=FVector(500,-500,0);
 UPROPERTY(BlueprintReadOnly) FVector LockedTransferPressurePa=FVector(0,500,-1000);
 // Current-transfer approval only; never written into recipe targets or recovery settings.
 UPROPERTY(BlueprintReadOnly) FGuid ReceiverPressureRequest;
 UPROPERTY(BlueprintReadOnly) int64 ReceiverPressureTransferId=0;
 UPROPERTY(BlueprintReadOnly) int64 ReceiverPressureRevision=0;
 UPROPERTY(BlueprintReadOnly) double ReceiverPressurePreviousPa=0;
 UPROPERTY(BlueprintReadOnly) double ReceiverPressureRequestedPa=0;
 UPROPERTY(BlueprintReadOnly) double ReceiverPressureConfirmedPa=0;
 UPROPERTY(BlueprintReadOnly) double ReceiverPressureRequestedSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double ReceiverPressureAppliedSeconds=-1;
 UPROPERTY(BlueprintReadOnly) bool ReceiverPressurePending=false;
 UPROPERTY(BlueprintReadOnly) FString ReceiverPressureStatus;
 UPROPERTY(BlueprintReadOnly) FGuid RunId;
 UPROPERTY(BlueprintReadOnly) FGuid PendingRequest;
 UPROPERTY(BlueprintReadOnly) int64 Epoch=-1;
 UPROPERTY(BlueprintReadOnly) int64 Connection=0;
 UPROPERTY(BlueprintReadOnly) EProcessAutoStage Stage=EProcessAutoStage::Idle;
 UPROPERTY(BlueprintReadOnly) bool Active=false;
 UPROPERTY(BlueprintReadOnly) FString Reason;
 UPROPERTY(BlueprintReadOnly) FString StopResult;
 UPROPERTY(BlueprintReadOnly) int64 BatchRevision=0;
 UPROPERTY(BlueprintReadOnly) int32 BatchCount=0;
 UPROPERTY(BlueprintReadOnly) double BatchKg=0;
 UPROPERTY(BlueprintReadOnly) int64 TransferId=0;
 UPROPERTY(BlueprintReadOnly) double StartedSimulationSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double ConnectorEmptySimulationSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double BaselineSimulationSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double OutletOpenSimulationSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double FirstArrivalSimulationSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double UICompleteSimulationSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double FinishedSimulationSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double RemainingKg=0;
 UPROPERTY(BlueprintReadOnly) double TransitKg=0;
 UPROPERTY(BlueprintReadOnly) double ArrivedKg=0;
 UPROPERTY(BlueprintReadOnly) int32 AdditionalInflowCount=0;
 UPROPERTY(BlueprintReadOnly) double PreparationStableSeconds=0;
 UPROPERTY(BlueprintReadOnly) bool DownstreamStarted=false;
 UPROPERTY(BlueprintReadOnly) double TargetMixerRPM=0;
 UPROPERTY(BlueprintReadOnly) double ReceiverClosedSimulationSeconds=-1;
 UPROPERTY(BlueprintReadOnly) int64 ReceiverCloseRevision=0;
 UPROPERTY(BlueprintReadOnly) double MixerRequestedSimulationSeconds=-1;
 int64 ConfirmedMixRun=0;
 double ConfirmedMixRPM=0;
 FGuid SpeedRequest;
 UPROPERTY(BlueprintReadOnly) int64 MixerRequestRevision=0;
 UPROPERTY(BlueprintReadOnly) double MixerAppliedSimulationSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double MixingDurationSeconds=0;
 UPROPERTY(BlueprintReadOnly) double MixingElapsedSeconds=0;
 UPROPERTY(BlueprintReadOnly) double MixingRemainingSeconds=0;
 UPROPERTY(BlueprintReadOnly) double MixingDeadlineSimulationSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double MixingTargetReachedSimulationSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double MixerStopRequestedSimulationSeconds=-1;
 UPROPERTY(BlueprintReadOnly) int64 MixerStopRequestRevision=0;
 UPROPERTY(BlueprintReadOnly) FGuid MixerStopRequestId;
 UPROPERTY(BlueprintReadOnly) double MixerStoppedSimulationSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double MixingStopOverrunSeconds=0;
};
