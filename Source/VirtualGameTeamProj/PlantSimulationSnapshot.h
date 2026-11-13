#pragma once

#include "CoreMinimal.h"
#include "PlantTransferTypes.h"
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

/** A copy of committed telemetry, never a view into GPU/worker memory. */
USTRUCT(BlueprintType)
struct FPlantSimulationSnapshot
{
    GENERATED_BODY()
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
    // RPM used by the last successful solver batch, not a tachometer reading.
    UPROPERTY(BlueprintReadOnly) double AppliedRPM = 0;
    UPROPERTY(BlueprintReadOnly) bool AppliedRPMValid = false;
    // Worker acknowledgement of one coherent control mailbox, scoped to Epoch.
    UPROPERTY(BlueprintReadOnly) int64 ProcessedControlRevision = 0;
    UPROPERTY(BlueprintReadOnly) int64 RequestedControlRevision = 0;
    UPROPERTY(BlueprintReadOnly) double WorkerTargetRPM = 15;
    UPROPERTY(BlueprintReadOnly) bool WorkerMixerEnabled = false;
    UPROPERTY(BlueprintReadOnly) bool PressureControlEnabled = false;
    UPROPERTY(BlueprintReadOnly) FVector RequestedGaugePressurePa = FVector::ZeroVector;
    // Boundary settings successfully uploaded by the worker, not measured chamber pressure.
    UPROPERTY(BlueprintReadOnly) FVector AppliedGaugePressurePa = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) bool AppliedPressureValid = false;
    UPROPERTY(BlueprintReadOnly) int64 PressureAppliedRevision = 0;
    UPROPERTY(BlueprintReadOnly) double WeighingRegulatorM3s = 0;
    UPROPERTY(BlueprintReadOnly) FPlantMassValue WeighingMass;
    UPROPERTY(BlueprintReadOnly) FPlantMassValue PipeMass;
    UPROPERTY(BlueprintReadOnly) FPlantMassValue ReceiverMass;
    UPROPERTY(BlueprintReadOnly) double ModelParticleMassKg = 0;
    UPROPERTY(BlueprintReadOnly) FString MassDiagnostic;
    UPROPERTY(BlueprintReadOnly) FPlantTransferSnapshot Transfer;
    UPROPERTY(BlueprintReadOnly) FPlantFeedPreparationSnapshot FeedPreparation;
    UPROPERTY(BlueprintReadOnly) FPlantSupplyAPurgeSnapshot SupplyAPurge;
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
