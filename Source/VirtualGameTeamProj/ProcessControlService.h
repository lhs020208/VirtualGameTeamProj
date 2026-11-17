#pragma once

#include "CoreMinimal.h"
#include "ProcessControlTypes.h"
#include "ProcessAutoRun.h"
#include "ProcessRecipe.h"
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
    FProcessAutoRun AutoRun;
    UPROPERTY(Config) FProcessAutoSettings AutoSettings;
    UFUNCTION(BlueprintPure,Category="Process UI|Automatic") FProcessAutoSnapshot GetAutomaticSnapshot() const { return AutoRun.State; }
    bool StartAutomaticA();
    UFUNCTION(BlueprintCallable) bool ResumeAutomaticBatch();
    UFUNCTION(BlueprintCallable) FString CheckAutomaticBatch();
    UFUNCTION(BlueprintCallable) void AbandonAutomaticBatch();
    bool ApplyRecipe(const FProcessRecipe& Recipe,FString& Reason);
    UFUNCTION(BlueprintPure,Category="Process UI|Recipe") FProcessRecipe GetAppliedRecipe() const;
    UPROPERTY(Transient) FProcessRecipe RecipeMetadata;
    UPROPERTY(BlueprintReadOnly,Transient) int64 RecipeRevision=0;
    bool SetAutomaticBatchMode(EProcessBatchMode Mode,FString& Reason);
    bool SetAutomaticTargetMass(double TargetKg,double ToleranceKg,FString& Reason);
    bool SetAutomaticAdditive(bool Enabled,int32 Supplier,bool Scheduled,double TargetKg,double ToleranceKg,double Delay,FString& Reason);
    bool SetAutomaticBTargetMass(double TargetKg,double ToleranceKg,FString& Reason);
    bool SetAutomaticMixingDuration(double Seconds,FString& Reason);
    void ConfirmAutomaticUI(int64 Epoch,int64 TransferId,double Time) { AutoRun.ConfirmUI(Epoch,TransferId,Time); }
    void CancelAutomatic(const FString& Reason);
    FProcessCommandRecord RequestTransferReceiverPressure(FGuid Id,int64 Epoch,int64 TransferId,double Pa);

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
