#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/HUD.h"
#include "GameFramework/GameModeBase.h"
#include "DEMSolver.h"
#include "DEMSimulationActor.generated.h"
class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
UCLASS(Blueprintable, BlueprintType)
class DEMPROTOTYPE_API ADEMSimulationActor : public AActor
{
    GENERATED_BODY()
  public:
    ADEMSimulationActor();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DEM|Reset settings") int32 ParticleCount = 200;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DEM|Reset settings") int32 Seed = 20261021;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DEM|Reset settings") double RadiusM = .01;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DEM|Reset settings") double Density = 1000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DEM|Reset settings") double NormalStiffness = 500000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DEM|Reset settings") double TangentStiffness = 250000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DEM|Reset settings") double DampingRatio = .2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DEM|Reset settings") double Friction = .4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DEM|Reset settings") double FixedDt = .00001;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DEM|Reset settings") bool PhysicsOnly = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DEM|Reset settings") bool HighResolution = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DEM|Runtime") double Speed = .05;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DEM|Runtime") double DisplayHz = 60;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DEM|Runtime") int32 MaxSubsteps = 500;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DEM|Runtime") bool ShowRotation = false;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Instances;
    UFUNCTION(BlueprintCallable, Category = "DEM") void ResetSimulation();
    UFUNCTION(BlueprintCallable, Category = "DEM") void TogglePause();
    UFUNCTION(BlueprintCallable, Category = "DEM") void SingleStep();
    UFUNCTION(BlueprintCallable, Category = "DEM") void SaveResults();
    // Display assets are editable independently of the double precision solver.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DEM|Appearance") TObjectPtr<UStaticMesh> ParticleMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DEM|Appearance") TObjectPtr<UMaterialInterface> ParticleMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DEM|Editor") bool EditorPreview = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DEM|Editor") bool UseLevelCamera = false;
    virtual void OnConstruction(const FTransform &Transform) override;
    dem::Solver Solver;
    bool Running = false;
    double Backlog = 0, WallSeconds = 0, PrepareMs = 0, UpdateMs = 0;
    int32 LastSubsteps = 0;
    FString DisplayText;
    virtual void Tick(float DeltaSeconds) override;

  protected:
    virtual void BeginPlay() override;

  private:
    TArray<FTransform> Transforms;
    uint64 LastRenderFrame = MAX_uint64;
    double DisplayClock = 0, TextClock = 0, SampleClock = 0, WallStart = 0;
    TArray<FString> Samples;
    bool Auto = false;
    int32 AutoFrames = 0, AutoSteps = 0;
    bool Snapshot = false, Dense = false;
    int32 AutoBatch = 100, AutoEnd = 350;
    FString RunTag;
    TArray<double> FrameTimes, PrepTimes, UpdateTimes, StepTimes;
    void UpdateInstances(bool Rebuild = false);
    void RefreshText();
};
UCLASS()
class DEMPROTOTYPE_API ADEMHUD : public AHUD
{
    GENERATED_BODY()
  public:
    virtual void DrawHUD() override;
    virtual void NotifyHitBoxClick(FName BoxName) override;
};
UCLASS()
class DEMPROTOTYPE_API ADEMGameMode : public AGameModeBase
{
    GENERATED_BODY()
  public:
    ADEMGameMode();
    virtual void BeginPlay() override;
};




