#pragma once

#include "CoreMinimal.h"
#include "PlantTransferTypes.generated.h"

UENUM(BlueprintType)
enum class EPlantTransferState : uint8 { Idle, Moving, Complete, Indeterminate, Empty };

/** Provisional observation criteria only. They never drive a valve or solver setting. */
USTRUCT(BlueprintType)
struct FPlantTransferSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) double WeighingResidualKg = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) double PipeResidualKg = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) double MassErrorKg = .000001; // 0.001 g
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) double StableSimulationSeconds = .5;
};
