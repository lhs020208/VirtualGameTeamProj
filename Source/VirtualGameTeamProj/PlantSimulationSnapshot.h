#pragma once

#include "CoreMinimal.h"
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
