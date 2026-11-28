#pragma once

#include "CoreMinimal.h"
#include "PlantTransferTypes.h"
#include "PlantBatchRecoveryTypes.h"
#include "PlantSimulationSnapshot.generated.h"

UENUM(BlueprintType)
enum class EPlantValueValidity : uint8
{
    Valid, Unknown, Initializing, Stale, SourceMissing, Unsupported, Error
};

UENUM(BlueprintType)
enum class EPlantExecutionState : uint8
{
    Unavailable, Initializing, Running, Paused, PauseRequested, ResumeRequested, Fault
};

USTRUCT(BlueprintType)
struct FPlantMassValue
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) double Kg = 0;
    UPROPERTY(BlueprintReadOnly) FString Unit = TEXT("kg");
    UPROPERTY(BlueprintReadOnly) EPlantValueValidity Validity = EPlantValueValidity::Unknown;
    UPROPERTY(BlueprintReadOnly) FDateTime UpdatedUtc;
    UPROPERTY(BlueprintReadOnly) FString Source = TEXT("시뮬레이션 계산값");
};

/** Worker-confirmed preparation for one opening revision, scoped to the actor epoch/connection. */
USTRUCT(BlueprintType)
struct FPlantFeedPreparationSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool Enabled = false;
    UPROPERTY(BlueprintReadOnly) int64 RequestRevision = 0;
    UPROPERTY(BlueprintReadOnly) int64 Connection = 0;
    UPROPERTY(BlueprintReadOnly) EPlantFeedPreparationState State = EPlantFeedPreparationState::Idle;
    UPROPERTY(BlueprintReadOnly) FString Reason;
    UPROPERTY(BlueprintReadOnly) int32 SupplyResidualCount = 0;
    UPROPERTY(BlueprintReadOnly) TArray<int32> SupplyResidualByPath;
    UPROPERTY(BlueprintReadOnly) double SupplyResidualKg = 0;
    UPROPERTY(BlueprintReadOnly) int32 WeighingCount = 0;
    UPROPERTY(BlueprintReadOnly) double WeighingKg = 0;
    UPROPERTY(BlueprintReadOnly) double StableSeconds = 0;
    UPROPERTY(BlueprintReadOnly) double RequiredStableSeconds = 0;
    UPROPERTY(BlueprintReadOnly) double RequestedSimulationSeconds = 0;
    UPROPERTY(BlueprintReadOnly) double ObservedSimulationSeconds = 0;
    UPROPERTY(BlueprintReadOnly) double SupplyConfirmedSimulationSeconds = -1;
    UPROPERTY(BlueprintReadOnly) double BaselineCapturedSimulationSeconds = -1;
    UPROPERTY(BlueprintReadOnly) double OutletAppliedSimulationSeconds = -1;
};

USTRUCT(BlueprintType)
struct FPlantTransferSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int64 Id = 0;
    UPROPERTY(BlueprintReadOnly) int64 Connection = 0;
    UPROPERTY(BlueprintReadOnly) int64 OpeningControlRevision = 0;
    UPROPERTY(BlueprintReadOnly) EPlantValueValidity Validity = EPlantValueValidity::Unknown;
    UPROPERTY(BlueprintReadOnly) EPlantTransferState State = EPlantTransferState::Idle;
    UPROPERTY(BlueprintReadOnly) FString Reason;
    UPROPERTY(BlueprintReadOnly) FDateTime UpdatedUtc;
    UPROPERTY(BlueprintReadOnly) double StartedSimulationSeconds = 0;
    // First worker-confirmed residence of a captured ID; independent of UI polling.
    UPROPERTY(BlueprintReadOnly) double FirstArrivalSimulationSeconds = -1;
    UPROPERTY(BlueprintReadOnly) double ObservedSimulationSeconds = 0;
    UPROPERTY(BlueprintReadOnly) double StableSeconds = 0;
    UPROPERTY(BlueprintReadOnly) double BaselineKg = 0;
    UPROPERTY(BlueprintReadOnly) double RemainingKg = 0;
    UPROPERTY(BlueprintReadOnly) double InTransitKg = 0;
    // Current residence of the captured IDs, not a cumulative crossing counter.
    UPROPERTY(BlueprintReadOnly) double ArrivedKg = 0;
    UPROPERTY(BlueprintReadOnly) double UnclassifiedKg = 0;
    // Unique IDs outside the captured cohort / existing receiver inventory; never added to baseline.
    UPROPERTY(BlueprintReadOnly) int32 AdditionalInflowCount = 0;
    UPROPERTY(BlueprintReadOnly) double InitialReceiverKg = 0;
    UPROPERTY(BlueprintReadOnly) bool OutletAppliedOpen = false;
    // Carrier flow from the field successfully uploaded to the solver, not a pressure/flow sensor.
    UPROPERTY(BlueprintReadOnly) double CalculatedCarrierFlowM3s = 0;
    UPROPERTY(BlueprintReadOnly) bool CarrierFlowValid = false;
    UPROPERTY(BlueprintReadOnly) bool CarrierRegulated = false;
    UPROPERTY(BlueprintReadOnly) FPlantTransferSettings Criteria;
};

/** Ideal gas-only A downstream boundary; applied fields are worker confirmations, not sensors. */
USTRUCT(BlueprintType)
struct FPlantSupplyAPurgeSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool Supported = false;
    UPROPERTY(BlueprintReadOnly) bool Valid = false;
    UPROPERTY(BlueprintReadOnly) bool RequestedEnabled = false;
    UPROPERTY(BlueprintReadOnly) double RequestedGaugePa = 0;
    UPROPERTY(BlueprintReadOnly) bool AppliedEnabled = false;
    UPROPERTY(BlueprintReadOnly) double AppliedGaugePa = 0;
    UPROPERTY(BlueprintReadOnly) double FlowM3s = 0;
    UPROPERTY(BlueprintReadOnly) int64 AppliedRevision = 0;
    UPROPERTY(BlueprintReadOnly) int64 CancelledRevision = 0;
    UPROPERTY(BlueprintReadOnly) FString Diagnostic;
    UPROPERTY(BlueprintReadOnly) int32 ResidualCount = 0;
    UPROPERTY(BlueprintReadOnly) double ResidualKg = 0;
    UPROPERTY(BlueprintReadOnly) double ObservedSimulationSeconds = 0;
    UPROPERTY(BlueprintReadOnly) FVector InletMetres = FVector::ZeroVector;
};

/** Current residence of the original A IDs during RH201 -> MX301, never cumulative crossings. */
USTRUCT(BlueprintType)
struct FPlantMixerFeedSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool Supported=false;
    UPROPERTY(BlueprintReadOnly) bool Valid=false;
    UPROPERTY(BlueprintReadOnly) EPlantTransferState State=EPlantTransferState::Idle;
    UPROPERTY(BlueprintReadOnly) FString Reason;
    UPROPERTY(BlueprintReadOnly) int64 OpeningRevision=0;
    UPROPERTY(BlueprintReadOnly) int64 RejectedRevision=0;
    UPROPERTY(BlueprintReadOnly) int64 CloseRevision=0;
    UPROPERTY(BlueprintReadOnly) int64 MixerStartRevision=0;
    UPROPERTY(BlueprintReadOnly) int64 MixerAppliedRevision=0;
    UPROPERTY(BlueprintReadOnly) double BaselineKg=0;
    UPROPERTY(BlueprintReadOnly) double ReceiverKg=0;
    UPROPERTY(BlueprintReadOnly) double ConnectorKg=0;
    UPROPERTY(BlueprintReadOnly) double MixerKg=0;
    UPROPERTY(BlueprintReadOnly) double InitialMixerKg=0;
    UPROPERTY(BlueprintReadOnly) int32 AdditionalInflowCount=0;
    UPROPERTY(BlueprintReadOnly) double StartedSimulationSeconds=-1;
    UPROPERTY(BlueprintReadOnly) double ObservedSimulationSeconds=0;
    UPROPERTY(BlueprintReadOnly) double FirstArrivalSimulationSeconds=-1;
    UPROPERTY(BlueprintReadOnly) double StableSeconds=0;
    UPROPERTY(BlueprintReadOnly) double CompleteSimulationSeconds=-1;
    UPROPERTY(BlueprintReadOnly) double ClosedSimulationSeconds=-1;
    UPROPERTY(BlueprintReadOnly) double MixerStartSimulationSeconds=-1;
    UPROPERTY(BlueprintReadOnly) double MixerAppliedSimulationSeconds=-1;
    UPROPERTY(BlueprintReadOnly) double AppliedRPM=0;
};

/** Initial charge IDs for supplier A, classified by the worker after completed GPU batches. */
USTRUCT(BlueprintType)
struct FPlantFeedBrakeSnapshot
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) bool Supported=false;
 UPROPERTY(BlueprintReadOnly) bool Valid=false;
 UPROPERTY(BlueprintReadOnly) bool Required=false;
 UPROPERTY(BlueprintReadOnly) bool Applied=false;
 UPROPERTY(BlueprintReadOnly) int64 AppliedRevision=0;
 UPROPERTY(BlueprintReadOnly) double AppliedSimulationSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double LowerFaceMetres=0;
 UPROPERTY(BlueprintReadOnly) double ResponseTimeSeconds=0;
 UPROPERTY(BlueprintReadOnly) int32 GateOccupancyCount=0;
 UPROPERTY(BlueprintReadOnly) int32 OccupancyAtRequest=0;
 UPROPERTY(BlueprintReadOnly) int32 GuardDeferrals=0;
 UPROPERTY(BlueprintReadOnly) int32 PredictedSpillCount=0;
 UPROPERTY(BlueprintReadOnly) double PredictedSpillKg=0;
 UPROPERTY(BlueprintReadOnly) double PredictedSpillAtRequestKg=0;
 UPROPERTY(BlueprintReadOnly) double RequestSimulationSeconds=-1;
 UPROPERTY(BlueprintReadOnly) int64 RequestRevision=0;
 UPROPERTY(BlueprintReadOnly) double BrakeAppliedPassedKg=0;
 UPROPERTY(BlueprintReadOnly) double GateClearSimulationSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double GateClearPassedKg=0;
 UPROPERTY(BlueprintReadOnly) double GateClosedSimulationSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double ObservedSimulationSeconds=-1;
};

// Worker-owned original-charge ID cohorts; per-material mass cannot offset another material's error.
USTRUCT(BlueprintType)
struct FPlantMaterialBatchSnapshot
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) int32 InitialCount=0;
 UPROPERTY(BlueprintReadOnly) int32 SourceCount=0;
 UPROPERTY(BlueprintReadOnly) int32 SelectedCount=0;
 UPROPERTY(BlueprintReadOnly) int32 ReceiverCount=0;
 UPROPERTY(BlueprintReadOnly) int32 MixerCount=0;
 UPROPERTY(BlueprintReadOnly) int32 ConnectorCount=0;
 UPROPERTY(BlueprintReadOnly) double TargetKg=0;
 UPROPERTY(BlueprintReadOnly) double ToleranceKg=0;
 UPROPERTY(BlueprintReadOnly) double FinalKg=0;
 UPROPERTY(BlueprintReadOnly) double FinalSimulationSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double ReceiverCompleteSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double FirstMixerSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double AllMixerSeconds=-1;
 UPROPERTY(BlueprintReadOnly) int64 CohortHash=0;
};
USTRUCT(BlueprintType)
struct FPlantSequentialBatchSnapshot
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) TArray<FPlantMaterialBatchSnapshot> Materials;
 bool RegularsInMixer() const
 {
  if(Materials.IsEmpty())return A.SelectedCount==A.MixerCount&&B.SelectedCount==B.MixerCount;
  for(int32 I=0;I<Materials.Num();++I)
   if((!AdditiveActive||I!=ActiveSupplier)&&Materials[I].SelectedCount!=Materials[I].MixerCount)return false;
  return true;
 }
 UPROPERTY(BlueprintReadOnly) FPlantMaterialBatchSnapshot A;
 UPROPERTY(BlueprintReadOnly) FPlantMaterialBatchSnapshot B;
 UPROPERTY(BlueprintReadOnly) FPlantMaterialBatchSnapshot Additive;
 UPROPERTY(BlueprintReadOnly) bool AdditiveActive=false;
 UPROPERTY(BlueprintReadOnly) bool AdditiveScheduled=false;
 UPROPERTY(BlueprintReadOnly) double AdditiveDelaySeconds=0;
 UPROPERTY(BlueprintReadOnly) bool AdditiveReady=false;
 UPROPERTY(BlueprintReadOnly) double AdditiveReadySeconds=-1;
 UPROPERTY(BlueprintReadOnly) int32 ActiveSupplier=0;
 UPROPERTY(BlueprintReadOnly) bool Combined=false;
 UPROPERTY(BlueprintReadOnly) int64 TransitionRevision=0;
 UPROPERTY(BlueprintReadOnly) int64 RejectedRevision=0;
 UPROPERTY(BlueprintReadOnly) FString Reason;
};
USTRUCT(BlueprintType)
struct FPlantABatchSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 InitialCount=0;
    UPROPERTY(BlueprintReadOnly) double InitialKg=0;
    UPROPERTY(BlueprintReadOnly) int32 InventorySourceCount=0;
    UPROPERTY(BlueprintReadOnly) int64 WeighingSetRevision=0;
    UPROPERTY(BlueprintReadOnly) double WeighingStableSeconds=0;
    UPROPERTY(BlueprintReadOnly) bool DoseFinalized=false;
    UPROPERTY(BlueprintReadOnly) int64 DoseFinalizeRevision=0;
    UPROPERTY(BlueprintReadOnly) int64 DoseRejectedRevision=0;
    UPROPERTY(BlueprintReadOnly) FString DoseReason;
    UPROPERTY(BlueprintReadOnly) double DoseFinalKg=0;
    UPROPERTY(BlueprintReadOnly) double DoseTargetKg=0;
    UPROPERTY(BlueprintReadOnly) double DoseToleranceKg=0;
    UPROPERTY(BlueprintReadOnly) double DoseFinalSimulationSeconds=-1;
    UPROPERTY(BlueprintReadOnly) int64 SupplyCloseRequestedRevision=0;
    UPROPERTY(BlueprintReadOnly) int64 SupplyClosedRevision=0;
    UPROPERTY(BlueprintReadOnly) double SupplyCloseRequestedSeconds=-1;
    UPROPERTY(BlueprintReadOnly) double SupplyClosedSeconds=-1;
    UPROPERTY(BlueprintReadOnly) double CloseRequestedWeighingKg=0;
    UPROPERTY(BlueprintReadOnly) double CloseRequestedConnectorKg=0;
    UPROPERTY(BlueprintReadOnly) double CloseRequestedPassedKg=0;
    UPROPERTY(BlueprintReadOnly) double ClosedWeighingKg=0;
    UPROPERTY(BlueprintReadOnly) double ClosedConnectorKg=0;
    UPROPERTY(BlueprintReadOnly) double ClosedPassedKg=0;
    UPROPERTY(BlueprintReadOnly) double ClosingExtraKg=0;

    UPROPERTY(BlueprintReadOnly) bool Supported = false;
    UPROPERTY(BlueprintReadOnly) bool Valid = false;
    UPROPERTY(BlueprintReadOnly) FString Error;
    UPROPERTY(BlueprintReadOnly) int64 CaptureRevision = 0;
    UPROPERTY(BlueprintReadOnly) int32 TotalCount = 0;
    UPROPERTY(BlueprintReadOnly) double TotalKg = 0;
    UPROPERTY(BlueprintReadOnly) double ObservedCohortKg = 0;
    UPROPERTY(BlueprintReadOnly) int32 SourceCount = 0;
    UPROPERTY(BlueprintReadOnly) int32 ConnectorCount = 0;
    UPROPERTY(BlueprintReadOnly) int32 WeighingCount = 0;
    UPROPERTY(BlueprintReadOnly) int32 PipeCount = 0;
    UPROPERTY(BlueprintReadOnly) int32 ReceiverCount = 0;
    UPROPERTY(BlueprintReadOnly) int32 MixerConnectorCount = 0;
    UPROPERTY(BlueprintReadOnly) int32 MixerCount = 0;
    UPROPERTY(BlueprintReadOnly) int32 InitialMixerCount = 0;
    UPROPERTY(BlueprintReadOnly) int32 UnknownCount = 0;
    UPROPERTY(BlueprintReadOnly) int32 ForeignCount = 0;
    UPROPERTY(BlueprintReadOnly) double CapturedSimulationSeconds = -1;
    UPROPERTY(BlueprintReadOnly) double ObservedSimulationSeconds = 0;
};

/** A copy of committed telemetry, never a view into GPU/worker memory. */
/** Immutable target + one-shot worker cutoff; physical closure remains guarded. */
USTRUCT(BlueprintType)
struct FPlantDoseCutoffSnapshot
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) bool Supported=false;
 UPROPERTY(BlueprintReadOnly) bool Valid=false;
 UPROPERTY(BlueprintReadOnly) bool Armed=false;
 UPROPERTY(BlueprintReadOnly) bool Triggered=false;
 UPROPERTY(BlueprintReadOnly) bool Cancelled=false;
 UPROPERTY(BlueprintReadOnly) int32 Supplier=-1;
 UPROPERTY(BlueprintReadOnly) int64 ArmRevision=0;
 UPROPERTY(BlueprintReadOnly) int64 BatchRevision=0;
 UPROPERTY(BlueprintReadOnly) int64 Epoch=-1;
 UPROPERTY(BlueprintReadOnly) int64 Connection=0;
 UPROPERTY(BlueprintReadOnly) int64 Sample=0;
 UPROPERTY(BlueprintReadOnly) int64 TriggerSample=0;
 UPROPERTY(BlueprintReadOnly) int64 TriggerPublicationSequence=0;
 UPROPERTY(BlueprintReadOnly) double TargetKg=0;
 UPROPERTY(BlueprintReadOnly) double ToleranceKg=0;
 UPROPERTY(BlueprintReadOnly) double RequestedWall=0;
 UPROPERTY(BlueprintReadOnly) double ReceivedWall=0;
 UPROPERTY(BlueprintReadOnly) double ReceivedSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double OpenedSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double ObservedSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double TriggerSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double TriggerWall=0;
 UPROPERTY(BlueprintReadOnly) double PassedKg=0;
 UPROPERTY(BlueprintReadOnly) double SpillKg=0;
 UPROPERTY(BlueprintReadOnly) double LeadKg=0;
 UPROPERTY(BlueprintReadOnly) double EstimatedKg=0;
 UPROPERTY(BlueprintReadOnly) double GateClosedSeconds=-1;
 UPROPERTY(BlueprintReadOnly) FGuid RequestId;
 UPROPERTY(BlueprintReadOnly) FString Reason;
};

/** Same-publication model estimate at the existing cell bracketing the actual supplier gate. */
USTRUCT(BlueprintType)
struct FPlantSupplyValvePressure
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) int32 Supplier=-1;
 UPROPERTY(BlueprintReadOnly) bool Mapped=false;
 UPROPERTY(BlueprintReadOnly) bool Valid=false;
 UPROPERTY(BlueprintReadOnly) int32 Route=-1;
 UPROPERTY(BlueprintReadOnly) int32 GateCell=-1;
 UPROPERTY(BlueprintReadOnly) int32 GateDataIndex=-1;
 UPROPERTY(BlueprintReadOnly) FVector BeforeMetres=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) FVector AfterMetres=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly) double BeforeGaugePa=0;
 UPROPERTY(BlueprintReadOnly) double AfterGaugePa=0;
 UPROPERTY(BlueprintReadOnly) double DifferencePa=0;
 UPROPERTY(BlueprintReadOnly) double HopperDifferencePa=0;
 UPROPERTY(BlueprintReadOnly) double RouteLossPa=0;
 UPROPERTY(BlueprintReadOnly) double BoundaryResidualPa=0;
 UPROPERTY(BlueprintReadOnly) bool FlowValid=false;
 UPROPERTY(BlueprintReadOnly) double FlowM3s=0;
 UPROPERTY(BlueprintReadOnly) FString Reason;
};

USTRUCT(BlueprintType)
struct FPlantMixerDriveSnapshot {
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) bool Supported=false;
 UPROPERTY(BlueprintReadOnly) bool Tripped=false;
 UPROPERTY(BlueprintReadOnly) bool Stopped=true;
 UPROPERTY(BlueprintReadOnly) double TargetRPM=0;
 UPROPERTY(BlueprintReadOnly) double ActualRPM=0;
 UPROPERTY(BlueprintReadOnly) double MotorTorqueNm=0;
 UPROPERTY(BlueprintReadOnly) double ContactTorqueNm=0;
 UPROPERTY(BlueprintReadOnly) double ElectricalWatts=0;
 UPROPERTY(BlueprintReadOnly) double EnergyJoules=0;
 UPROPERTY(BlueprintReadOnly) double PowerFraction=0;
 UPROPERTY(BlueprintReadOnly) double NormalLoadN=0;
};

USTRUCT(BlueprintType)
struct FPlantValveActuatorSnapshot {
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) double AngleDegrees=0;
 UPROPERTY(BlueprintReadOnly) double AngularSpeedDegrees=0;
 UPROPERTY(BlueprintReadOnly) double MotorTorqueNm=0;
 UPROPERTY(BlueprintReadOnly) double ContactTorqueNm=0;
 UPROPERTY(BlueprintReadOnly) double NormalLoadN=0;
 UPROPERTY(BlueprintReadOnly) double ElectricalWatts=0;
 UPROPERTY(BlueprintReadOnly) double EnergyJoules=0;
 UPROPERTY(BlueprintReadOnly) double Voltage=24;
 UPROPERTY(BlueprintReadOnly) bool Closed=true;
 UPROPERTY(BlueprintReadOnly) bool Open=false;
 UPROPERTY(BlueprintReadOnly) bool Tripped=false;
};

USTRUCT(BlueprintType)
struct FPlantSimulationSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool EquipmentOnly=false;
    UPROPERTY(BlueprintReadOnly) int32 TotalParticleCount=0;
    UPROPERTY(BlueprintReadOnly) TArray<FPlantValveActuatorSnapshot> ValveActuators;
    UPROPERTY(BlueprintReadOnly) FPlantMixerDriveSnapshot MixerDrive;
    // Command acknowledgement is separate from tachometer speed during ramp/load changes.
    bool MixerTargetApplied(double Target) const {
        if(!AppliedRPMValid)return false;
        if(!MixerDrive.Supported)return AppliedRPM==double(float(Target));
        return !MixerDrive.Tripped&&FMath::Abs(MixerDrive.TargetRPM-Target)<.001&&
            (Target==0?MixerDrive.Stopped:(MixerDrive.PowerFraction>0&&AppliedRPM>0));
    }

    UPROPERTY(BlueprintReadOnly) FPlantBatchRecoverySnapshot Recovery;
    UPROPERTY(BlueprintReadOnly) FPlantABatchSnapshot SupplyABatch;
    UPROPERTY(BlueprintReadOnly) FPlantDoseCutoffSnapshot DoseCutoff;
    UPROPERTY(BlueprintReadOnly) FPlantFeedBrakeSnapshot SupplyABrake;
    UPROPERTY(BlueprintReadOnly) FPlantFeedBrakeSnapshot SupplyBBrake;
    UPROPERTY(BlueprintReadOnly) FPlantFeedBrakeSnapshot SupplyCBrake;
    UPROPERTY(BlueprintReadOnly) FPlantFeedBrakeSnapshot SupplyDBrake;
    UPROPERTY(BlueprintReadOnly) FPlantFeedBrakeSnapshot SupplyEBrake;
    UPROPERTY(BlueprintReadOnly) FPlantFeedBrakeSnapshot SupplyFBrake;
    UPROPERTY(BlueprintReadOnly) FPlantSequentialBatchSnapshot SequentialBatch;
    UPROPERTY(BlueprintReadOnly) FPlantMixerFeedSnapshot MixerFeed;
    UPROPERTY(BlueprintReadOnly) FPlantMixerFeedSnapshot InitialBatchMixerFeed;
    UPROPERTY(BlueprintReadOnly) FString GPUDevice;
    UPROPERTY(BlueprintReadOnly) int64 Epoch = 0;
    UPROPERTY(BlueprintReadOnly) int64 Sequence = 0;
    UPROPERTY(BlueprintReadOnly) FDateTime UpdatedUtc;
    // Monotonic wall time for freshness; deliberately independent of simulation time.
    double PublishedWallSeconds = 0;
    UPROPERTY(BlueprintReadOnly) EPlantValueValidity Validity = EPlantValueValidity::Initializing;
    UPROPERTY(BlueprintReadOnly) EPlantExecutionState Execution = EPlantExecutionState::Initializing;
    UPROPERTY(BlueprintReadOnly) FString SourceActorPath;
    UPROPERTY(BlueprintReadOnly) FString Error;
    UPROPERTY(BlueprintReadOnly) double PhysicalSeconds = 0;
    UPROPERTY(BlueprintReadOnly) int32 InWeighingHopper = 0;
    UPROPERTY(BlueprintReadOnly) int32 InReceiver = 0;
    UPROPERTY(BlueprintReadOnly) int32 InMixer = 0;
    UPROPERTY(BlueprintReadOnly) TArray<int32> InitialSupplierCounts;
    UPROPERTY(BlueprintReadOnly) TArray<bool> RequestedSuppliers;
    UPROPERTY(BlueprintReadOnly) int32 AppliedSupplierMask = 0;
    UPROPERTY(BlueprintReadOnly) bool RequestedHopperOpen = false;
    UPROPERTY(BlueprintReadOnly) bool AppliedHopperOpen = false;
    UPROPERTY(BlueprintReadOnly) bool RequestedReceiverOpen = false;
    UPROPERTY(BlueprintReadOnly) bool AppliedReceiverOpen = false;
    UPROPERTY(BlueprintReadOnly) bool RequestedRunning = false;
    UPROPERTY(BlueprintReadOnly) bool RequestedMixerEnabled = false;
    UPROPERTY(BlueprintReadOnly) double RequestedRPM = 0;
    // Computed shaft speed when powered drive is enabled; legacy prescribed RPM otherwise.
    UPROPERTY(BlueprintReadOnly) double AppliedRPM = 0;
    UPROPERTY(BlueprintReadOnly) bool AppliedRPMValid = false;
    // First successful GPU batch using this mailbox revision; reset with the worker/epoch.
    UPROPERTY(BlueprintReadOnly) int64 AppliedRPMRevision = 0;
    UPROPERTY(BlueprintReadOnly) double AppliedRPMSimulationSeconds = -1;
    // Worker acknowledgement of one coherent control mailbox, scoped to Epoch.
    UPROPERTY(BlueprintReadOnly) int64 ProcessedControlRevision = 0;
    UPROPERTY(BlueprintReadOnly) int64 RequestedControlRevision = 0;
    UPROPERTY(BlueprintReadOnly) double WorkerTargetRPM = 15;
    UPROPERTY(BlueprintReadOnly) bool WorkerMixerEnabled = false;
    UPROPERTY(BlueprintReadOnly) bool PressureControlEnabled = false;
    UPROPERTY(BlueprintReadOnly) TArray<double> RequestedSupplierPa;
    UPROPERTY(BlueprintReadOnly) TArray<double> AppliedSupplierPa;
    double SupplierPressure(int32 I,bool Applied) const {
        const auto& Values=Applied?AppliedSupplierPa:RequestedSupplierPa;
        return Values.IsValidIndex(I)?Values[I]:(Applied?AppliedGaugePressurePa.X:RequestedGaugePressurePa.X);
    }
    bool SuppliersVented() const {
        for(double Pa:RequestedSupplierPa)if(Pa!=0)return false;
        for(double Pa:AppliedSupplierPa)if(Pa!=0)return false;
        return true;
    }
    UPROPERTY(BlueprintReadOnly) FVector RequestedGaugePressurePa = FVector::ZeroVector;
    // Boundary settings successfully uploaded by the worker, not measured chamber pressure.
    UPROPERTY(BlueprintReadOnly) FVector AppliedGaugePressurePa = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) bool AppliedPressureValid = false;
    UPROPERTY(BlueprintReadOnly) int64 PressureAppliedRevision = 0;
    UPROPERTY(BlueprintReadOnly) double PressureAppliedSimulationSeconds = -1;
    UPROPERTY(BlueprintReadOnly) int64 TransferPressureRejectedRevision = 0;
    UPROPERTY(BlueprintReadOnly) double WeighingRegulatorM3s = 0;
    UPROPERTY(BlueprintReadOnly) TArray<FPlantSupplyValvePressure> SupplyValvePressures;
    UPROPERTY(BlueprintReadOnly) double MixerAngleRadians=0;
    int64 MixRun=0, SpeedAppliedRevision=0, SpeedRejectedRevision=0;
    double SpeedAppliedSeconds=-1, SpeedStartAngleRadians=0;
    UPROPERTY(BlueprintReadOnly) double AppliedRPMStartAngleRadians=0;
    UPROPERTY(BlueprintReadOnly) FPlantMassValue WeighingMass;
    UPROPERTY(BlueprintReadOnly) FPlantMassValue PipeMass;
    UPROPERTY(BlueprintReadOnly) FPlantMassValue ReceiverMass;
    UPROPERTY(BlueprintReadOnly) int32 InTransferPipe=0;
    UPROPERTY(BlueprintReadOnly) double MixerMassKg=0;
    UPROPERTY(BlueprintReadOnly) TArray<double> SupplierParticleMassKg;
    double SourceKg(int32 I) const {return SupplierParticleMassKg.IsValidIndex(I)?SupplierParticleMassKg[I]:ModelParticleMassKg;}
    UPROPERTY(BlueprintReadOnly) double ModelParticleMassKg = 0;
    UPROPERTY(BlueprintReadOnly) FString MassDiagnostic;
    UPROPERTY(BlueprintReadOnly) FPlantTransferSnapshot Transfer;
    UPROPERTY(BlueprintReadOnly) FPlantFeedPreparationSnapshot FeedPreparation;
    UPROPERTY(BlueprintReadOnly) FPlantSupplyAPurgeSnapshot SupplyAPurge;
    UPROPERTY(BlueprintReadOnly) FPlantSupplyAPurgeSnapshot SupplyBPurge;
    UPROPERTY(BlueprintReadOnly) FPlantSupplyAPurgeSnapshot SupplyCPurge;
    UPROPERTY(BlueprintReadOnly) FPlantSupplyAPurgeSnapshot SupplyDPurge;
    UPROPERTY(BlueprintReadOnly) FPlantSupplyAPurgeSnapshot SupplyEPurge;
    UPROPERTY(BlueprintReadOnly) FPlantSupplyAPurgeSnapshot SupplyFPurge;
};

USTRUCT(BlueprintType)
struct FProcessStateValue
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) double Value = 0;
    UPROPERTY(BlueprintReadOnly) FString Unit;
    UPROPERTY(BlueprintReadOnly) EPlantValueValidity Validity = EPlantValueValidity::Unknown;
};

USTRUCT(BlueprintType)
struct FProcessEquipmentSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FPlantMixerDriveSnapshot MixerDrive;
    UPROPERTY(BlueprintReadOnly) bool HasValveActuator=false;
    UPROPERTY(BlueprintReadOnly) FPlantValveActuatorSnapshot ValveActuator;

    UPROPERTY(BlueprintReadOnly) FString EquipmentId;
    UPROPERTY(BlueprintReadOnly) FString Source = TEXT("실시간 시뮬레이션 · 미보정");
    UPROPERTY(BlueprintReadOnly) FString SourceActorPath;
    UPROPERTY(BlueprintReadOnly) EPlantValueValidity Validity = EPlantValueValidity::Unknown;
    UPROPERTY(BlueprintReadOnly) EPlantExecutionState Execution = EPlantExecutionState::Unavailable;
    UPROPERTY(BlueprintReadOnly) FString Error;
    UPROPERTY(BlueprintReadOnly) FDateTime UpdatedUtc;
    UPROPERTY(BlueprintReadOnly) int64 Sequence = 0;
    UPROPERTY(BlueprintReadOnly) int64 Epoch = 0;
    UPROPERTY(BlueprintReadOnly) FProcessStateValue SimulationTime;
    UPROPERTY(BlueprintReadOnly) FProcessStateValue RegionParticleCount;
    UPROPERTY(BlueprintReadOnly) FProcessStateValue InitialParticleCount;
    UPROPERTY(BlueprintReadOnly) FProcessStateValue ValveRequestedOpen;
    UPROPERTY(BlueprintReadOnly) FProcessStateValue ValveAppliedOpen;
    UPROPERTY(BlueprintReadOnly) FProcessStateValue RequestedRPM;
    UPROPERTY(BlueprintReadOnly) FProcessStateValue AppliedRPM;
    UPROPERTY(BlueprintReadOnly) FProcessStateValue RequestedPressurePa;
    UPROPERTY(BlueprintReadOnly) FProcessStateValue AppliedPressurePa;
    UPROPERTY(BlueprintReadOnly) FString PressureScope;
    UPROPERTY(BlueprintReadOnly) FPlantMassValue CurrentMass;
    UPROPERTY(BlueprintReadOnly) FPlantTransferSnapshot Transfer;
    UPROPERTY(BlueprintReadOnly) FPlantFeedPreparationSnapshot FeedPreparation;
    UPROPERTY(BlueprintReadOnly) FPlantSupplyAPurgeSnapshot SupplyAPurge;
    UPROPERTY(BlueprintReadOnly) FPlantSupplyAPurgeSnapshot SupplyBPurge;
    UPROPERTY(BlueprintReadOnly) FPlantSupplyAPurgeSnapshot SupplyCPurge;
    UPROPERTY(BlueprintReadOnly) FPlantSupplyAPurgeSnapshot SupplyDPurge;
    UPROPERTY(BlueprintReadOnly) FPlantSupplyAPurgeSnapshot SupplyEPurge;
    UPROPERTY(BlueprintReadOnly) FPlantSupplyAPurgeSnapshot SupplyFPurge;
};

/** Per-binding ordering guard. Reset only when changing the explicit source. */
struct FProcessSnapshotCursor
{
    int64 Epoch = -1, Sequence = -1;
    bool Accept(int64 NewEpoch, int64 NewSequence)
    {
        if (NewEpoch < Epoch || (NewEpoch == Epoch && NewSequence < Sequence)) return false;
        Epoch = NewEpoch;
        Sequence = NewSequence;
        return true;
    }
};

// A/B/C are the verified metering branches. Callers reject other indices before dispatch.
inline const FPlantSupplyAPurgeSnapshot& PlantPurgeFor(const FPlantSimulationSnapshot& S,int32 I){check(I>=0&&I<6); switch(I){case 0:return S.SupplyAPurge;case 1:return S.SupplyBPurge;case 2:return S.SupplyCPurge;case 3:return S.SupplyDPurge;case 4:return S.SupplyEPurge;default:return S.SupplyFPurge;}}
inline FPlantSupplyAPurgeSnapshot& PlantPurgeFor(FPlantSimulationSnapshot& S,int32 I){check(I>=0&&I<6); switch(I){case 0:return S.SupplyAPurge;case 1:return S.SupplyBPurge;case 2:return S.SupplyCPurge;case 3:return S.SupplyDPurge;case 4:return S.SupplyEPurge;default:return S.SupplyFPurge;}}
inline const FPlantFeedBrakeSnapshot& PlantBrakeFor(const FPlantSimulationSnapshot& S,int32 I){check(I>=0&&I<6); switch(I){case 0:return S.SupplyABrake;case 1:return S.SupplyBBrake;case 2:return S.SupplyCBrake;case 3:return S.SupplyDBrake;case 4:return S.SupplyEBrake;default:return S.SupplyFBrake;}}

template<typename Predicate>
inline bool AnyPlantPurge(const FPlantSimulationSnapshot& S,Predicate Matches)
{
 for(int32 I=0;I<6;++I)if(Matches(PlantPurgeFor(S,I)))return true;
 return false;
}

inline FString PlantValveStatus(const FPlantValveActuatorSnapshot& A){return A.Tripped?TEXT("과부하 정지"):A.Closed?TEXT("닫힘"):A.Open?TEXT("열림"):FString::Printf(TEXT("회전 중 %.1f°"),A.AngleDegrees);}
