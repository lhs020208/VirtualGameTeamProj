#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "powder/granular.hpp"
#include "PowderGranularMixer.generated.h"
class ADEMEmitter;class UDEMParticleSettings;class UInstancedStaticMeshComponent;class UStaticMeshComponent;struct FGranularRuntime;
UCLASS()
class VIRTUALGAMETEAMPROJ_API APowderGranularMixer:public AActor {
 GENERATED_BODY()
public:
 APowderGranularMixer();~APowderGranularMixer();
 virtual void OnConstruction(const FTransform&)override;virtual void BeginPlay()override;virtual void EndPlay(const EEndPlayReason::Type)override;virtual void Tick(float)override;
 UPROPERTY(EditAnywhere,Category="Granular|Setup",meta=(EditCondition="!UseEmitters",ClampMin="1",ClampMax="1000000")) int32 ParticleCount=30000;
 UPROPERTY(EditAnywhere,Category="Granular|Emitters") bool UseEmitters=false;
 UPROPERTY(EditAnywhere,Category="Granular|Materials",meta=(ToolTip="Opt-in experimental two-material position solver. Not calibrated pharmaceutical DEM.")) bool BinaryMaterials=false;
 UPROPERTY(EditAnywhere,Category="Granular|Materials",meta=(ClampMin="0",ClampMax="1")) double CrossMaterialFriction=.4;
 UPROPERTY(EditAnywhere,Category="Granular|Materials",meta=(EditCondition="BinaryMaterials")) bool EnableParticleRotation=false;
 UPROPERTY(EditAnywhere,Category="Granular|Materials",meta=(EditCondition="EnableParticleRotation",ClampMin="0",ClampMax="1")) double RollingFriction=0;
 UPROPERTY(EditAnywhere,Category="Granular|Materials",meta=(EditCondition="BinaryMaterials",ToolTip="Explicit opt-in to conservative max-radius candidate reuse and kernel argument caching.")) bool MaterialOptimizations=false;
 UFUNCTION(BlueprintPure,Category="Granular|Materials") int32 ParticleCountForMass(UDEMParticleSettings* Settings,double MassKg) const;
 powder::GranularConfig EmitterConfig;
 UPROPERTY(EditAnywhere,Category="Granular|Emitters",meta=(ToolTip="Explicit source order defines stable particle IDs. Legacy mode requires matching properties; binary mode allows one material per group.")) TArray<TObjectPtr<ADEMEmitter>> Emitters;
 std::vector<powder::Grain> CopyParticleState();
 bool BuildEmitterInitialState(std::vector<powder::Grain>& Positions,std::vector<powder::Grain>& Velocities);
 UPROPERTY(EditAnywhere,Category="Granular|Setup",meta=(ToolTip="Optional explicit GPU approximation settings. Read on Reset. Null preserves legacy settings.")) TObjectPtr<UDEMParticleSettings> ParticleSettings;
 UPROPERTY(EditAnywhere,Category="Granular|Setup",meta=(ToolTip="Legacy mode mass accounting only, kg/m3. Does not calibrate dynamics.")) double DensityKgPerCubicMeter=1200;
 UPROPERTY(VisibleAnywhere,Category="Granular|Status") FString SettingsStatus;
 UFUNCTION(CallInEditor,BlueprintCallable,Category="Granular") bool ValidateSimulationSettings();
 UPROPERTY(EditAnywhere,Category="Granular|Setup",meta=(ClampMin="2",ClampMax="32")) int32 ContactIterations=16;
 UPROPERTY(EditAnywhere,Category="Granular|Setup") double TimeStep=.00025;
 UPROPERTY(EditAnywhere,Category="Granular|Setup") bool TestMixerAsset=false;
 UPROPERTY(EditAnywhere,Category="Granular|Setup",meta=(ClampMin="0",ClampMax="1")) double Friction=.4;
 UPROPERTY(EditAnywhere,Category="Granular|Setup",meta=(ClampMin="1",ClampMax="128")) int32 SolverBatchSize=32;
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
 UPROPERTY(VisibleAnywhere,Category="Granular|Status") double ChargeMassKg=0;
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
 FGranularRuntime* Runtime=nullptr;uint64 Generation=0;bool Benchmark=false,Shot1=false,Shot2=false;double FinishWall=0;FString OutputDir;double BenchmarkSeconds=12;
 bool PendingEmitterStart=false,SourceResetRequired=false;
 TArray<TWeakObjectPtr<ADEMEmitter>> ActiveEmitters;
 TArray<int32> ActiveSourceCounts;
 FTransform ActiveMixerFrame;
 double ActiveParticleMassKg=0;
 void Stop();void Geometry();void Display();
};
