#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "PowderDropTest.generated.h"

class UStaticMeshComponent;
class UPhysicalMaterial;
class ACameraActor;
class UPrimitiveComponent;
class AStaticMeshActor;

/** Enlarged powder proxies: gravity/contact test only, not molecular dynamics. */
UCLASS()
class VIRTUALGAMETEAMPROJ_API APowderDropTest : public AActor
{
    GENERATED_BODY()
public:
    APowderDropTest();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    void ResetDrop();
    void TogglePause();
    void FocusTube();
    int32 TubeEntered = 0;
    int32 TubePassed = 0;
    int32 TubeInside = 0;
    int32 TubeReturned = 0;
    int32 TubeWallContacts = 0;
    static constexpr float TubeInnerRadius = 12.f;
    static constexpr float TubeBottom = 240.f;
    static constexpr float TubeTop = 360.f;
    static FVector TubeCenterXY() { return FVector(0, 0, 0); }
    float Elapsed = 0.f;
    float MeanHeight = 0.f;
    int32 MovingCount = 0;
    int32 EscapedCount = 0;
    bool bReleased = false;
    int32 ObstacleContacts = 0;
    int32 TouchedObstacleCount = 0;
    int32 MultiObstacleParticleCount = 0;
    static constexpr float PlatformHalfSize = 1000.f;
    static FVector OverviewOffset() { return FVector(650, -950, 640); }
    static FVector OverviewTarget() { return FVector(0, 0, 340); }
    static constexpr int32 ParticleCount = 300;
    static constexpr float RadiusCm = 3.f;
    static constexpr float FloorTop = 12.f;
private:
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Particles;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Platform;
    UPROPERTY() TObjectPtr<UPhysicalMaterial> ContactMaterial;
    UPROPERTY() TObjectPtr<ACameraActor> Camera;
    TArray<FTransform> InitialTransforms;
    bool bCameraReady = false;
    float NextReport = 0.f;
    UPROPERTY() TArray<TObjectPtr<AStaticMeshActor>> Obstacles;
    UPROPERTY() TObjectPtr<UPhysicalMaterial> ObstacleMaterial;
    TSet<AActor*> TouchedObstacles;
    TMap<UPrimitiveComponent*, TSet<AActor*>> ParticleObstacleHistory;
    void BuildObstacleCourse();
    void UpdateTubeTracking();
    bool IsWithinTubeBore(const FVector& LocalPosition) const;
    struct FTubeParticleState
    {
        FVector Previous = FVector::ZeroVector;
        bool bEverEntered = false;
        bool bPassed = false;
        bool bActive = false;
        bool bReturned = false;
        bool bExitPending = false;
    };
    TArray<FTubeParticleState> TubeStates;
    UFUNCTION()
    void RecordObstacleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);
};

UCLASS()
class VIRTUALGAMETEAMPROJ_API APowderDropHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};

UCLASS()
class VIRTUALGAMETEAMPROJ_API APowderDropGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    APowderDropGameMode();
    virtual void StartPlay() override;
};
