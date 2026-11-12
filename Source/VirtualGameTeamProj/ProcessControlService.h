#pragma once

#include "CoreMinimal.h"
#include "ProcessControlTypes.h"
#include "ProcessControlService.generated.h"

class APowderPlantTest;

/** Single control owner for the explicitly bound plant. UI never writes actor/worker state. */
UCLASS(Config=Game)
class VIRTUALGAMETEAMPROJ_API UProcessControlService : public UObject
{
    GENERATED_BODY()
public:
    bool Attach(APowderPlantTest* Actor);
    void Detach();
    void Tick();
    FProcessCommandRecord Request(const FString& Equipment, EProcessCommand Command, double Value, FGuid RequestId = FGuid());
    FProcessCommandRecord RequestForEpoch(FGuid RequestId, int64 Epoch, const FString& Equipment, EProcessCommand Command, double Value);
    FProcessControlMachine State;
    // Development trial limits in monotonic wall seconds; not certified operating limits.
    UPROPERTY(Config) double CommandTimeoutSeconds = 10;
    UPROPERTY(Config) double StopTimeoutSeconds = 30;
    // User-approved policies for ordinary play, also exercised without test overrides.
    UPROPERTY(Config) FString StopFailurePolicy = TEXT("Freeze");
    UPROPERTY(Config) FString ManualPausePolicy = TEXT("CloseThenPause");
    UPROPERTY(Config) FString PausedClosePolicy = TEXT("StopAll");
    APowderPlantTest* GetSource() const { return Source.Get(); }
private:
    TWeakObjectPtr<APowderPlantTest> Source;
    FProcessControlTransport Transport();
};
