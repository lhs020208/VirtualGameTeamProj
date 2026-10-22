#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PowderGranularMixer.generated.h"
class UInstancedStaticMeshComponent;class UStaticMeshComponent;struct FGranularRuntime;
UCLASS()
class VIRTUALGAMETEAMPROJ_API APowderGranularMixer:public AActor {
 GENERATED_BODY()
public:
 APowderGranularMixer();~APowderGranularMixer();
 virtual void OnConstruction(const FTransform&)override;virtual void BeginPlay()override;virtual void EndPlay(const EEndPlayReason::Type)override;virtual void Tick(float)override;
 UPROPERTY(EditAnywhere,Category="Granular|Setup",meta=(ClampMin="30000",ClampMax="1000000")) int32 ParticleCount=30000;
 UPROPERTY(EditAnywhere,Category="Granular|Setup",meta=(ClampMin="2",ClampMax="32")) int32 ContactIterations=16;
 UPROPERTY(EditAnywhere,Category="Granular|Setup") double TimeStep=.00025;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Granular|Control") bool Running=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Granular|Control",meta=(ClampMin="0",ClampMax="60")) double RPM=30;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Granular|Control",meta=(ClampMin="0.01",ClampMax="2")) double TargetRate=.1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Granular|Control") bool GateOpen=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Granular|Control") bool AutomaticSequence=true;
 UPROPERTY(EditAnywhere,Category="Granular|Control") double MixUntilSeconds=9;
 UPROPERTY(VisibleAnywhere,Category="Granular|Status") double PhysicalSeconds=0;
 UPROPERTY(VisibleAnywhere,Category="Granular|Status") double ActualRate=0;
 UPROPERTY(VisibleAnywhere,Category="Granular|Status") double RadiusMm=0;
 UPROPERTY(VisibleAnywhere,Category="Granular|Status") double MixingIndex=0;
 UPROPERTY(VisibleAnywhere,Category="Granular|Status") double ChargeMassKg=64.8;
 UPROPERTY(VisibleAnywhere,Category="Granular|Status") double ReceiverMassKg=0;
 UPROPERTY(VisibleAnywhere,Category="Granular|Status") int32 ReceiverCount=0;
 UPROPERTY(VisibleAnywhere,Category="Granular|Status") FString Error;
 UPROPERTY(VisibleAnywhere,Category="Granular|Status") FString Phase;
 UFUNCTION(CallInEditor,BlueprintCallable,Category="Granular") void ResetSimulation();
private:
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> GroupA;
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> GroupB;
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Walls;
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Blades;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Shaft;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> GateMesh;
 FGranularRuntime* Runtime=nullptr;uint64 Generation=0;bool Benchmark=false,Shot1=false,Shot2=false;double FinishWall=0;FString OutputDir;
 void Stop();void Geometry();void Display();
};
