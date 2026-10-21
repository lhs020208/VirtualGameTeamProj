#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/HUD.h"
#include "PowderDEMTest.generated.h"

namespace powder {struct PowderSettings;}
UENUM(BlueprintType)
enum class EDEMSizeDistribution : uint8 { Mono, UniformNumber };
UENUM(BlueprintType)
enum class EDEMAmountMode : uint8 { Count, Mass };
UENUM(BlueprintType)
enum class EDEMSupplyMode : uint8 { Batch, Timed };
UENUM(BlueprintType)
enum class EDEMContactLaw : uint8 { Linear, HertzMindlin };
class UInstancedStaticMeshComponent;
class ACameraActor;
struct FDEMRuntime;
struct FDEMRuntimeDeleter {void operator()(FDEMRuntime* P) const;};

UCLASS()
class VIRTUALGAMETEAMPROJ_API APowderDEMTest : public AActor
{
 GENERATED_BODY()
public:
 APowderDEMTest();
 virtual ~APowderDEMTest() override;
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 UFUNCTION(BlueprintCallable,Category="DEM") void ResetSimulation();
 UFUNCTION(BlueprintCallable,Category="DEM|Settings") bool SaveSettings();
 UFUNCTION(BlueprintCallable,Category="DEM|Settings") bool LoadSettings();
 void SaveSettingsKey(){SaveSettings();}
 void LoadSettingsKey(){LoadSettings();}
 UFUNCTION(BlueprintCallable,Category="DEM|Results") bool ExportResults();
 void ExportResultsKey(){ExportResults();}
 FString ResultStatus() const;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="DEM|Results") FString ExportStatus;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="DEM|Results") FString LastResultsDirectory;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Results") double ResultSampleSeconds=.01;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Results") double SlowSpeedMps=.001;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Results") double LongStallSeconds=1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Results",meta=(ClampMin="2",ClampMax="100000")) int32 MaxResultSamples=10000;
 FString InitialStatus() const;
 FString SupplyStatus() const;
 int32 RequestedCount() const;
 int32 PendingCount() const;
 double SupplyMassError() const;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Supply") EDEMSupplyMode SupplyMode=EDEMSupplyMode::Batch;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Supply",meta=(ClampMin="0")) double SupplyStartSeconds=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Supply",meta=(EditCondition="SupplyMode == EDEMSupplyMode::Timed")) double SupplyIntervalSeconds=.01;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Supply",meta=(ClampMin="1",ClampMax="300",EditCondition="SupplyMode == EDEMSupplyMode::Timed")) int32 ParticlesPerEvent=1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Supply",meta=(EditCondition="SupplyMode == EDEMSupplyMode::Timed")) double SupplyRetrySeconds=.001;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="DEM|Settings") FString SettingsStatus;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Settings") FString SettingsFile=TEXT("PowderSettings.dem");
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Settings") EDEMSizeDistribution SizeDistribution=EDEMSizeDistribution::Mono;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Settings") EDEMAmountMode AmountMode=EDEMAmountMode::Count;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Settings") EDEMContactLaw ContactLaw=EDEMContactLaw::Linear;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Settings",meta=(ClampMin="0")) int64 RandomSeed=42;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Settings",meta=(EditCondition="SizeDistribution == EDEMSizeDistribution::UniformNumber")) double MinDiameterM=.03;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Settings",meta=(EditCondition="SizeDistribution == EDEMSizeDistribution::UniformNumber")) double MaxDiameterM=.05;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Settings",meta=(EditCondition="AmountMode == EDEMAmountMode::Mass")) double TargetMassKg=10;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Settings",meta=(EditCondition="AmountMode == EDEMAmountMode::Mass")) double MassToleranceKg=.05;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Settings") FVector SpawnHalfExtentsCm=FVector(18.5,18.5,35);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Settings") FVector InitialVelocityMps=FVector::ZeroVector;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Settings") FVector GravityMps2=FVector(0,0,-9.81);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Settings") double SpacingFactor=1.25;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Settings") double InitialWallClearanceM=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Settings",meta=(ClampMin="1",ClampMax="1000000")) int32 MaxPlacementCandidates=100000;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Material") double ParticleYoungPa=1e7;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Material") double ParticlePoisson=.25;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Material") double WallYoungPa=2e11;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Material") double WallPoisson=.3;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|WallLinear") double WallNormalStiffness=4e6;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|WallLinear") double WallNormalDamping=40;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|WallLinear") double WallTangentStiffness=1e6;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|WallLinear") double WallTangentDamping=10;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|WallLinear") double WallFriction=.3;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|PairHertz") double HertzNormalDamping=40;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|PairHertz") double HertzTangentDamping=10;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|PairHertz") double HertzFriction=.3;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|PairHertz") double HertzOverlapCap=.01;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|WallHertz") double WallHertzNormalDamping=40;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|WallHertz") double WallHertzTangentDamping=10;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|WallHertz") double WallHertzFriction=.3;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|WallHertz") double WallHertzOverlapCap=.01;
 UFUNCTION(BlueprintCallable,Category="DEM") void ToggleSimulation();
 UFUNCTION(BlueprintCallable,Category="DEM") void StepSimulation();
 UFUNCTION(BlueprintCallable,Category="DEM") void PauseSimulation();
 UFUNCTION(BlueprintPure,Category="DEM|Tracking") FString ParticleTracking(int64 Id) const;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Tracking",meta=(ClampMin="1")) int32 InspectParticleId=1;
 FString TrackingStatus() const;
 FString PassageStatus() const;
 bool TrackingEnabled() const;
 int32 TrackedCount(int32 Region) const;
 double TrackedMassError() const;
 bool IsReady() const;
 int32 DisplayedCount() const;
 uint64 RunGeneration() const;
 void FocusTube();
 FVector ParticlePosition() const;
 FVector SolverPositionCm() const;
 bool IsRunning() const;
 uint64 StepCount() const;
 double SimulationSeconds() const;
 double RealSeconds() const;
 double ComputeSeconds() const {return ComputeTime;}
 int32 ContactCount() const;
 int32 HistoryCount() const;
 bool ChaosDisabled() const;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="DEM") FString Error;
 // Synthetic geometry-test particle, not calibrated powder. SI inputs except position.
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Settings",meta=(ClampMin="1",ClampMax="300",EditCondition="AmountMode == EDEMAmountMode::Count")) int32 ParticleCount=300;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Settings",meta=(ClampMin="0.000001",EditCondition="SizeDistribution == EDEMSizeDistribution::Mono")) double DiameterM=.04;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Settings",meta=(ClampMin="0.000001")) double DensityKgM3=2500;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Settings",meta=(DisplayName="Spawn Centre (cm)")) FVector InitialPositionCm=FVector(0,0,485);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Settings",meta=(ClampMin="0.000000001")) double FixedDt=2e-7;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|PairLinear") double NormalStiffness=4e6;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|PairLinear") double NormalDamping=40;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|PairLinear") double TangentStiffness=1e6;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|PairLinear") double TangentDamping=10;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|PairLinear") double Friction=.3;
private:
 powder::PowderSettings MakeSettings() const;
 void ApplySettings(const powder::PowderSettings& Settings);
 FString SettingsPath() const;
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> ParticleMesh;
 UPROPERTY() TObjectPtr<ACameraActor> Camera;
 TUniquePtr<FDEMRuntime,FDEMRuntimeDeleter> Runtime;
 bool bInputReady=false;
 double ResetTime=0,ComputeTime=0;
 void SyncParticle();
};

UCLASS()
class VIRTUALGAMETEAMPROJ_API APowderDEMHUD : public AHUD
{
 GENERATED_BODY()
public:
 virtual void DrawHUD() override;
};
