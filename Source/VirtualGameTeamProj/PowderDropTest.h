#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "PowderDropTest.generated.h"

class UStaticMeshComponent;
class UPhysicalMaterial;
class ACameraActor;

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
    float Elapsed = 0.f;
    float MeanHeight = 0.f;
    int32 MovingCount = 0;
    int32 EscapedCount = 0;
    bool bReleased = false;
    static constexpr int32 ParticleCount = 100;
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
