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
    bool ApplyParticleCounts(const TArray<int32>& Counts,FString& Reason);
    static bool LoadParticleCounts(const FString& DataDirectory,TArray<int32>& Counts);
    void Detach();
    void Tick();
    FProcessCommandRecord Request(const FString& Equipment, EProcessCommand Command, double Value, FGuid RequestId = FGuid());
    FProcessCommandRecord RequestForEpoch(FGuid RequestId, int64 Epoch, const FString& Equipment, EProcessCommand Command, double Value);
    // Shared operator admission. Web lease is renewed only by an authenticated relay heartbeat.
    FString WebLeaseId;
    double WebLeaseUntil=0;
    int64 WebLeaseEpoch=-1;
    bool WebDispatch=false;
    bool WebOwnsControl() const;
    FString LocalOperatorReason() const;
    FProcessCommandRecord RequestWeb(FGuid Id,int64 Epoch,const FString& Lease,const FString& Equipment,EProcessCommand Command,double Value);
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
    FProcessCommandRecord RequestMixSpeed(FGuid Id,int64 Epoch,FGuid Batch,int64 MixRun,double RPM,const FString* WebLease=nullptr);
    FProcessCommandRecord RequestStagePressure(FGuid Id,int64 Epoch,FGuid Batch,FGuid Stage,int32 Supplier,int64 TransferId,const FString& Equipment,double Pa,const FString* WebLease=nullptr);
    FProcessCommandRecord RequestTransferReceiverPressure(FGuid Id,int64 Epoch,int64 TransferId,double Pa,FGuid ExpectedBatch=FGuid(),FString ExpectedLease=FString());
    FString WebTransferPressureReason(int64 Epoch,const FString& Lease,FGuid Batch,int64 TransferId,double Now) const;
    FProcessCommandRecord RequestWebTransferReceiverPressure(FGuid Id,int64 Epoch,const FString& Lease,FGuid Batch,int64 TransferId,double Pa);

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
