#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlantSimulationSnapshot.h"
#include "ProcessControlTypes.h"
#include "PowderPlantTest.generated.h"
class UInstancedStaticMeshComponent;
class APlantVisibilityController;
class UProcessControlService;
struct FPlantRuntime;
class SWidget;
USTRUCT()
struct FPlantRotorVisual {
 GENERATED_BODY()
 UPROPERTY() TObjectPtr<AActor> Actor;
 UPROPERTY() FTransform Initial;
};
UCLASS()
class VIRTUALGAMETEAMPROJ_API APowderPlantTest:public AActor {
 GENERATED_BODY()
public:
 APowderPlantTest();~APowderPlantTest();
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 UPROPERTY(EditAnywhere,Category="Plant|Test",meta=(ClampMin="0",ClampMax="4")) int32 SectionTest=0;
 UPROPERTY(EditAnywhere,Category="Plant|Test") FString DataDirectory=TEXT("SourceAssets/ProcessPlantPhysics");
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Plant|Control") bool Running=true;
 UPROPERTY(meta=(DeprecatedProperty,DeprecationMessage="Simulation now runs without a rate cap; this saved value is ignored.")) double TargetRate=.1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Plant|Control",meta=(ClampMin="0",ClampMax="3000")) double TestPressureDropPa=1000;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Plant|Control",meta=(ClampMin="0",ClampMax="30")) double MixerRPM=15;
 UPROPERTY(EditAnywhere,Category="Plant|Test",meta=(ClampMin="0.000001",ClampMax="0.000025")) double FixedDt=.00001;
 UPROPERTY(EditAnywhere,Category="Plant|Test",meta=(ClampMin="4",ClampMax="32")) int32 ContactIterations=8;
 UPROPERTY(EditAnywhere,Category="Plant|Test",meta=(ToolTip="Physical seconds before an incomplete stage stops with a diagnostic, never teleports particles.")) double StageTimeoutSeconds=60;
 UPROPERTY(EditAnywhere,Category="Plant|Test",meta=(ClampMin="0.01",ClampMax="1",ToolTip="Test valve schedule only. Remaining particles are retained, never discarded.")) double MinimumReceiverFraction=.98;
 UPROPERTY(EditAnywhere,Category="Plant|Test",meta=(ClampMin="0")) double ReceiverTransferSeconds=2.;
 UPROPERTY(EditAnywhere,Category="Plant|Transfer Observation",meta=(ToolTip="Provisional model-mass completion criteria, not an approved operating standard. Applied on initialization. Never controls valves.")) FPlantTransferSettings TransferSettings;
 UPROPERTY(VisibleAnywhere,Category="Plant|Status") int32 ParticleCount=0;
 UPROPERTY(VisibleAnywhere,Category="Plant|Status") int32 InWeighingHopper=0;
 UPROPERTY(VisibleAnywhere,Category="Plant|Status") int32 InReceiver=0;
 UPROPERTY(VisibleAnywhere,Category="Plant|Status") int32 InMixer=0;
 UPROPERTY(VisibleAnywhere,Category="Plant|Status") double PhysicalSeconds=0;
 UPROPERTY(VisibleAnywhere,Category="Plant|Status") double ActualRate=0;
 UPROPERTY(VisibleAnywhere,Category="Plant|Status") FString Stage;
 UPROPERTY(VisibleAnywhere,Category="Plant|Status") FString Error;
 UFUNCTION(CallInEditor,BlueprintCallable,Category="Plant") void ResetSimulation();
 UFUNCTION(CallInEditor,BlueprintCallable,Category="Plant") void ExportSnapshot();
 UFUNCTION(BlueprintPure,Category="Plant|Read Only") FPlantSimulationSnapshot GetSimulationSnapshot() const;
 TSharedRef<SWidget> BuildSimulationPanel();
 UFUNCTION(BlueprintCallable,Category="Plant") void ToggleSupplier(int32 Index);
 UFUNCTION(BlueprintCallable,Category="Plant") void ToggleWeighingHopper();
 UFUNCTION(BlueprintCallable,Category="Plant") void ToggleReceiver();
 UFUNCTION(BlueprintCallable,Category="Plant") void ToggleMixer();
private:
 friend class UProcessControlService;
 TWeakObjectPtr<UProcessControlService> ControlOwner;
 FPlantControlTargets ControlTargets;
 bool ClaimControl(UProcessControlService* ServiceOwner);
 void ReleaseControl(UProcessControlService* ServiceOwner);
 bool ValidateControl(int64 Epoch, FString& Reason) const;
 bool SetSupplierValve(int32 Index, bool Open, int64 Epoch, int64& Revision, FString& Reason);
 bool SetWeighingValve(bool Open, int64 Epoch, int64& Revision, FString& Reason);
 bool SetReceiverValve(bool Open, int64 Epoch, int64& Revision, FString& Reason);
 bool SetMixerEnabled(bool Enabled, int64 Epoch, int64& Revision, FString& Reason);
 bool SetMixerRPM(double RPM, int64 Epoch, int64& Revision, FString& Reason);
 bool SetSimulationRunning(bool Run, int64 Epoch, int64& Revision, FString& Reason);
 bool RequestAllClosed(int64 Epoch, int64& Revision, FString& Reason);
 void PublishControls();
 bool LegacyControlsAllowed() const;
 int64 SnapshotEpoch=0;
 TArray<int32> InitialSupplierCounts;
 float ParticleRadiusM=.00125f;
 TArray<bool> SupplierOpen;
 bool WeighingHopperOpen=false,AppliedHopperOpen=false;
 bool ReceiverOpen=false,AppliedReceiverOpen=false,MixerEnabled=false;
 uint32 AppliedSupplierMask=0;int32 CurrentPhase=0;
 void InitializeSuppliers();
 UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Groups;
 UPROPERTY() TArray<FPlantRotorVisual> Rotor;
 UPROPERTY() TArray<TObjectPtr<AActor>> ValveVisuals;
 UPROPERTY() TObjectPtr<APlantVisibilityController> Display;
 FPlantRuntime* Runtime=nullptr;
 double LastShow=0;uint64 Generation=0;
 double SmokeSeconds=0,SmokeWallLimit=120,SmokeStart=0;bool SmokeShot=false;
 float PreviousMaxFPS=0;
 void Stop();void Show();void CacheRotor();
};
