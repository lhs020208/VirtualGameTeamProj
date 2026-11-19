#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PowderPlantTest.generated.h"
class UInstancedStaticMeshComponent;
class APlantVisibilityController;
struct FPlantRuntime;
class SWidget;
UENUM(BlueprintType)
enum class EPlantComputeBackend : uint8 {
 OpenCL UMETA(DisplayName="OpenCL"),
 Cuda UMETA(Hidden),
 CudaGraph UMETA(Hidden),
 CudaPtx UMETA(DisplayName="CUDA (preserved arithmetic)"),
 CudaPtxGraph UMETA(DisplayName="CUDA Graph (preserved arithmetic)")
};
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
 // Existing levels keep OpenCL. The isolated CUDA level opts in explicitly.
 UPROPERTY(EditAnywhere,Category="Plant|Compute") EPlantComputeBackend ComputeBackend=EPlantComputeBackend::OpenCL;
 UPROPERTY(VisibleAnywhere,Category="Plant|Status") FString ComputeDevice;
 // Opt-in only: old OpenCL/CUDA maps retain their original solver.
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Plant|Demo") bool DemoRealtime=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Plant|Demo") bool DemoAutoCycle=false;
 // Presentation only. Apply changes with ResetSimulation; physical input stays unchanged.
 UPROPERTY(EditAnywhere,Category="Plant|Demo",meta=(ClampMin="1",ClampMax="10")) int32 DemoVisualCopies=10;
 UPROPERTY(VisibleAnywhere,Category="Plant|Status") int32 DisplayedParticleCount=0;
 UPROPERTY(VisibleAnywhere,Category="Plant|Status") int32 SolverParticleCount=0;
 // Blender-world metres. Per-level geometry settings; legacy levels retain defaults.
 UPROPERTY(EditAnywhere,Category="Plant|Geometry") FVector MixerPivotM=FVector(3,0,.9);
 UPROPERTY(EditAnywhere,Category="Plant|Geometry") FVector MixerBoundsMinM=FVector(2.25,-.45,.57);
 UPROPERTY(EditAnywhere,Category="Plant|Geometry") FVector MixerBoundsMaxM=FVector(3.75,.45,1.31);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Plant|Control") bool Running=true;
 UPROPERTY(meta=(DeprecatedProperty,DeprecationMessage="Simulation now runs without a rate cap; this saved value is ignored.")) double TargetRate=.1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Plant|Control",meta=(ClampMin="0",ClampMax="3000")) double TestPressureDropPa=1000;
 UPROPERTY(EditAnywhere,Category="Plant|Pressure") bool RegulatedPressureEnabled=false;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Plant|Pressure") double SupplyGaugePa=0;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Plant|Pressure") double WeighingGaugePa=0;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Plant|Pressure") double ReceiverGaugePa=0;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Plant|Pressure") int32 PressureStage=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Plant|Pressure") bool PressureManualOverride=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Plant|Pressure") FVector AppliedGaugePa=FVector::ZeroVector;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Plant|Pressure") double WeighingRegulatorM3s=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Plant|Pressure") double TransferFlowM3s=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Plant|Pressure") bool PressureApplied=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Plant|Pressure") FString PressureMessage;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Plant|Pressure") FString RunLogPath;
 UFUNCTION(BlueprintCallable,Category="Plant|Pressure") bool SetPressureTargets(double SupplyPa,double WeighingPa,double ReceiverPa);
 UFUNCTION(BlueprintCallable,Category="Plant|Pressure") bool ApplyPressurePreset(int32 StageIndex);
 UFUNCTION(BlueprintCallable,Category="Plant|Pressure") bool SavePressureProfile();
 UFUNCTION(BlueprintCallable,Category="Plant|Pressure") bool LoadPressureProfile();
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Plant|Control",meta=(ClampMin="0",ClampMax="30")) double MixerRPM=15;
 UPROPERTY(EditAnywhere,Category="Plant|Test",meta=(ClampMin="0.000001",ClampMax="0.000025")) double FixedDt=.00001;
 UPROPERTY(EditAnywhere,Category="Plant|Test",meta=(ClampMin="4",ClampMax="32")) int32 ContactIterations=8;
 UPROPERTY(EditAnywhere,Category="Plant|Test",meta=(ToolTip="Physical seconds before an incomplete stage stops with a diagnostic, never teleports particles.")) double StageTimeoutSeconds=60;
 UPROPERTY(EditAnywhere,Category="Plant|Test",meta=(ClampMin="0.01",ClampMax="1",ToolTip="Test valve schedule only. Remaining particles are retained, never discarded.")) double MinimumReceiverFraction=.98;
 UPROPERTY(EditAnywhere,Category="Plant|Test",meta=(ClampMin="0")) double ReceiverTransferSeconds=2.;
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
 UFUNCTION(BlueprintCallable,Category="Plant") bool ExportSnapshotToFile(const FString& FilePath);
 TSharedRef<SWidget> BuildSimulationPanel();
 UFUNCTION(BlueprintCallable,Category="Plant") void ToggleSupplier(int32 Index);
 UFUNCTION(BlueprintCallable,Category="Plant") void ToggleWeighingHopper();
 UFUNCTION(BlueprintCallable,Category="Plant") void ToggleReceiver();
 UFUNCTION(BlueprintCallable,Category="Plant") void ToggleMixer();
private:
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
 TArray<FVector> VisualOffsets;
 FVector VisualScale=FVector::OneVector;
 TArray<TArray<FTransform>> InstanceTransforms;
 FPlantRuntime* Runtime=nullptr;
 double LastShow=0;uint64 Generation=0;
 double SmokeSeconds=0,SmokeWallLimit=120,SmokeStart=0;bool SmokeShot=false;
 int32 SmokeWarmFrames=0;
 float PreviousMaxFPS=0;
 double DemoTestSeconds=0,DemoTestStart=0,DemoLastFrame=0,DemoNextSample=0;
 int32 DemoWarmFrames=0,DemoAutoStage=-1,DemoShotStage=0,DemoCameraStage=0;
 TArray<double> DemoFrames;
 FString DemoOutput,DemoTrace;
 void TickDemo(double DeltaSeconds);
 void Stop();void Show();void CacheRotor();
 TSharedRef<SWidget> BuildPressurePanel();
};
