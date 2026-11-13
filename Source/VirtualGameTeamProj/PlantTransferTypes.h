#pragma once

#include "CoreMinimal.h"
#include "PlantTransferTypes.generated.h"

UENUM(BlueprintType)
enum class EPlantTransferState : uint8 { Idle, Moving, Complete, Indeterminate, Empty };

UENUM(BlueprintType)
enum class EPlantFeedPreparationState : uint8
{
    Idle, WaitingValves, WaitingSupplyResidual, WaitingWeighingStability,
    Empty, Ready, Opened, Cancelled, Failed, WaitingPurgeStop
};

/** Provisional mass-completion criteria. LowPoly requested-opening readiness also uses the stable interval.
 * No target-mass cutoff or physics parameter is derived from these values. */
USTRUCT(BlueprintType)
struct FPlantTransferSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) double WeighingResidualKg = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) double PipeResidualKg = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) double MassErrorKg = .000001; // 0.001 g
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) double StableSimulationSeconds = .5;
};
