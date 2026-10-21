#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "PowderClumpTest.generated.h"
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class ACameraActor;
class UMaterialInterface;
struct FClumpRuntime;
struct FClumpDeleter {void operator()(FClumpRuntime*)const;};
UCLASS()
class VIRTUALGAMETEAMPROJ_API APowderClumpTest:public AActor {
 GENERATED_BODY()
public:
 APowderClumpTest();virtual ~APowderClumpTest()override;
 virtual void OnConstruction(const FTransform& Transform)override;
 virtual void BeginPlay()override;virtual void Tick(float DeltaSeconds)override;
 UFUNCTION(BlueprintCallable,CallInEditor,Category="Clump") void ResetSimulation();
 UFUNCTION(BlueprintCallable,Category="Clump") void ToggleSimulation();
 UFUNCTION(BlueprintCallable,Category="Clump") void StepSimulation();
 UFUNCTION(BlueprintCallable,Category="Clump") bool ExportState();
 void ExportKey(){ExportState();}
 int32 RenderedPebbles()const;int32 BodyCount()const;FVector BodyPositionCm(int32 Index)const;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Geometry") bool bTriangleMeshRig=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Clump|Geometry") FString GeometryDiagnostics;
 int32 MeshTriangleCount()const;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Paddle") bool bPaddleRig=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Paddle") double PaddleRPM=30;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Clump|Paddle") double WallWorkJ=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Clump|Paddle") FVector WallReactionForceN=FVector::ZeroVector;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Clump|Paddle") FVector WallReactionTorqueNm=FVector::ZeroVector;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Clump|Paddle") double WallLoadTimeSeconds=0;

 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Mixing") bool bMixingRig=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Mixing",meta=(ClampMin="1",ClampMax="128")) int32 GroupACount=16;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Mixing",meta=(ClampMin="1",ClampMax="128")) int32 GroupBCount=16;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Mixing") double GroupBDensity=2500;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Mixing") bool bStartPremixed=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Mixing") double MixingDurationSeconds=.5;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Mixing") double MixingSampleSeconds=.05;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Mixing") FIntVector MixingGrid=FIntVector(2,2,1);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Mixing") int32 MinimumBodiesPerCell=4;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Mixing") TObjectPtr<UMaterialInterface> GroupAMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Mixing") TObjectPtr<UMaterialInterface> GroupBMaterial;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Clump|Mixing") bool bMixingIndexValid=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Clump|Mixing") bool bMixingFinished=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Clump|Mixing") double MixingIndex=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Clump|Mixing") double ChargeMassKg=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Clump|Mixing") FString MixingStatus;
 int32 RenderedGroupB()const;
 int32 MixingSampleCount()const;
 FQuat DisplayedPaddleRotation()const;
 double AppliedPaddleRPM()const;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Materials") double PowderDensity=2500;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Materials") double InclusionDensity=7800;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Materials") double PowderStiffness=10000;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Materials") double MixedStiffness=14000;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Materials") double InclusionStiffness=20000;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Materials") bool bHertz=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Materials") double PowderYoungPa=1e7;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Materials") double InclusionYoungPa=2e11;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Materials") double RollingFriction=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Materials") double PairAdhesionGamma=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Materials") double WallAdhesionGamma=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Simulation") double FixedDt=2e-6;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Simulation",meta=(ClampMin="1",ClampMax="5000")) int32 StepsPerFrame=200;
 // Real-time pacing preserves FixedDt; the budget only limits UI stalls.
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Simulation") bool bRealtimePacing=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Simulation",meta=(ClampMin="0.01",ClampMax="10")) double TargetSimulationRate=1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Clump|Simulation",meta=(ClampMin="1",ClampMax="100")) double ComputeBudgetMs=50;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Clump|Simulation") double ActualSimulationRate=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Clump|Simulation") double SimulationLagSeconds=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Clump") FString Error;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Clump") FString ExportDirectory;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Clump") double PhysicalSeconds=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Clump") bool bRunning=false;
private:
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Pebbles;
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> GroupBPebbles;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Floor;
 UPROPERTY() TObjectPtr<ACameraActor> Camera;
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> MeshObstacles;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Paddle;
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> TrayWalls;
 TUniquePtr<FClumpRuntime,FClumpDeleter> Runtime;
 double PacingLastWallSeconds=0, PacingTargetSeconds=0, RateWindowWallSeconds=0, RateWindowPhysicalSeconds=0;
 bool Advance();void Sync();
};
UCLASS()
class VIRTUALGAMETEAMPROJ_API APowderClumpGameMode:public AGameModeBase {
 GENERATED_BODY()
public:APowderClumpGameMode();virtual void StartPlay()override;
};
