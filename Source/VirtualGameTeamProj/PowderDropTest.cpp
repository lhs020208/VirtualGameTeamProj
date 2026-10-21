#include "PowderDropTest.h"
#include "PowderCameraController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"
#include "InputCoreTypes.h"
#include "DrawDebugHelpers.h"

APowderDropTest::APowderDropTest()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    Platform = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TestFloor"));
    Platform->SetupAttachment(RootComponent);
    Platform->SetStaticMesh(Cube.Object);
    Platform->SetRelativeLocation(FVector(0, 0, 10));
    Platform->SetRelativeScale3D(FVector(PlatformHalfSize / 50.f, PlatformHalfSize / 50.f, .04));
    Platform->SetCollisionProfileName(TEXT("BlockAll"));
    for (int32 I = 0; I < ParticleCount; ++I)
    {
        auto* P = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Powder_%03d"), I));
        P->SetupAttachment(RootComponent);
        P->SetStaticMesh(Sphere.Object);
        P->SetRelativeScale3D(FVector(RadiusCm / 50.f));
        P->SetCollisionProfileName(TEXT("PhysicsActor"));
        P->SetEnableGravity(true);
        P->SetLinearDamping(0.f); // No drag: the initial fall follows g = 980 cm/s^2.
        P->SetAngularDamping(3.f); // Simple rotational dissipation after contact; no linear air drag.
        P->BodyInstance.bUseCCD = true;
        Particles.Add(P);
    }
}

void APowderDropTest::BeginPlay()
{
    Super::BeginPlay();
    ContactMaterial = NewObject<UPhysicalMaterial>(this);
    ContactMaterial->Friction = .65f;
    ContactMaterial->Restitution = .15f;
    ContactMaterial->bOverrideFrictionCombineMode = true;
    ContactMaterial->FrictionCombineMode = EFrictionCombineMode::Average;
    ContactMaterial->bOverrideRestitutionCombineMode = true;
    ContactMaterial->RestitutionCombineMode = EFrictionCombineMode::Min;
    Platform->SetPhysMaterialOverride(ContactMaterial);
    BuildObstacleCourse();
    FRandomStream Random(100);
    for (int32 I = 0; I < ParticleCount; ++I)
    {
        // 5 x 5 x 12; 8 cm spacing exceeds the 6 cm diameter. No initial overlaps.
        const FVector P((I % 5 - 2) * 8.f + Random.FRandRange(-.4f, .4f),
            ((I / 5) % 5 - 2) * 8.f + Random.FRandRange(-.4f, .4f),
            450.f + (I / 25) * 9.f);
        InitialTransforms.Add(FTransform(FQuat::Identity, GetActorLocation() + P, FVector(RadiusCm / 50.f)));
        Particles[I]->SetPhysMaterialOverride(ContactMaterial);
        Particles[I]->SetNotifyRigidBodyCollision(true);
        Particles[I]->OnComponentHit.AddDynamic(this, &APowderDropTest::RecordObstacleHit);
        Particles[I]->SetMassOverrideInKg(NAME_None, .01f, true);
    }
    Camera = GetWorld()->SpawnActor<ACameraActor>();
    const FVector CameraPosition = GetActorLocation() + OverviewOffset();
    Camera->SetActorLocation(CameraPosition);
    Camera->SetActorRotation((GetActorLocation() + OverviewTarget() - CameraPosition).Rotation());
    Camera->GetCameraComponent()->SetFieldOfView(55.f);
    ResetDrop();
}

void APowderDropTest::ResetDrop()
{
    if (auto* PC = GetWorld()->GetFirstPlayerController()) PC->SetPause(false);
    Elapsed = 0.f;
    NextReport = 0.f;
    bReleased = false;
    ObstacleContacts = TouchedObstacleCount = MultiObstacleParticleCount = 0;
    TouchedObstacles.Reset();
    ParticleObstacleHistory.Reset();
    TubeEntered = TubePassed = TubeInside = TubeReturned = TubeWallContacts = 0;
    TubeStates.Init(FTubeParticleState(), ParticleCount);
    for (int32 I = 0; I < ParticleCount; ++I) TubeStates[I].Previous = InitialTransforms[I].GetLocation() - GetActorLocation();
    EscapedCount = 0;
    for (int32 I = 0; I < Particles.Num(); ++I)
    {
        UStaticMeshComponent* P = Particles[I];
        P->SetPhysicsLinearVelocity(FVector::ZeroVector);
        P->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        P->SetSimulatePhysics(false);
        P->SetWorldTransform(InitialTransforms[I], false, nullptr, ETeleportType::TeleportPhysics);
    }
    UE_LOG(LogTemp, Display, TEXT("PowderDrop RESET: count=%d radius=3cm mass=0.01kg gravity=%.1fcm/s2 seed=100"), Particles.Num(), GetWorld()->GetGravityZ());
}

void APowderDropTest::TogglePause()
{
    if (auto* PC = GetWorld()->GetFirstPlayerController()) PC->SetPause(!UGameplayStatics::IsGamePaused(this));
}

void APowderDropTest::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bCameraReady)
    {
        if (auto* PC = GetWorld()->GetFirstPlayerController())
        {
            PC->bAutoManageActiveCameraTarget = false;
            PC->SetViewTarget(Camera);
            EnableInput(PC);
            InputComponent->BindKey(EKeys::R, IE_Pressed, this, &APowderDropTest::ResetDrop).bExecuteWhenPaused = true;
            InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &APowderDropTest::TogglePause).bExecuteWhenPaused = true;
            InputComponent->BindKey(EKeys::T, IE_Pressed, this, &APowderDropTest::FocusTube).bExecuteWhenPaused = true;
            bCameraReady = true;
        }
    }
    Elapsed += DeltaSeconds;
    if (!bReleased && Elapsed >= 1.5f)
    {
        for (UStaticMeshComponent* P : Particles)
        {
            P->SetSimulatePhysics(true);
            P->SetPhysicsLinearVelocity(FVector::ZeroVector);
            P->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
            P->WakeAllRigidBodies();
        }
        bReleased = true;
        UE_LOG(LogTemp, Display, TEXT("PowderDrop RELEASE: %d particles, zero initial velocity"), ParticleCount);
    }
    MeanHeight = 0.f;
    MovingCount = 0;
    EscapedCount = 0;
    float MinZ = BIG_NUMBER;
    float MaxSpeed = 0.f;
    for (UStaticMeshComponent* P : Particles)
    {
        const FVector Position = P->GetComponentLocation() - GetActorLocation();
        const float Speed = P->GetPhysicsLinearVelocity().Size();
        MeanHeight += Position.Z - FloorTop - RadiusCm;
        MinZ = FMath::Min(MinZ, static_cast<float>(Position.Z));
        MaxSpeed = FMath::Max(MaxSpeed, Speed);
        if (Speed > 1.f) ++MovingCount;
        if (Position.ContainsNaN() || Position.Z < FloorTop - RadiusCm || FMath::Abs(Position.X) > PlatformHalfSize || FMath::Abs(Position.Y) > PlatformHalfSize) ++EscapedCount;
    }
    MeanHeight /= ParticleCount;
    UpdateTubeTracking();
    if (Elapsed >= NextReport)
    {
        UE_LOG(LogTemp, Display, TEXT("PowderDrop CHECK: t=%.2f count=%d moving=%d escaped=%d meanClearance=%.2fcm minCenterZ=%.2fcm maxSpeed=%.2fcm/s"),
            Elapsed, Particles.Num(), MovingCount, EscapedCount, MeanHeight, MinZ, MaxSpeed);
        UE_LOG(LogTemp, Display, TEXT("PowderCourse CHECK: contacts=%d fixtureShapes=%d multiContactParticles=%d/300"),
            ObstacleContacts, TouchedObstacleCount, MultiObstacleParticleCount);
        UE_LOG(LogTemp, Display, TEXT("PowderTube CHECK: entered=%d passed=%d inside=%d returned=%d wallContacts=%d rate=%.1f%%"),
            TubeEntered, TubePassed, TubeInside, TubeReturned, TubeWallContacts,
            TubeEntered ? 100.f * TubePassed / TubeEntered : 0.f);
        NextReport = Elapsed + 2.f;
    }
}

void APowderDropHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas) return;
    APowderDropTest* Test = nullptr;
    for (TActorIterator<APowderDropTest> It(GetWorld()); It; ++It) { Test = *It; break; }
    if (!Test) return;
    const float W = FMath::Min(530.f, Canvas->SizeX - 32.f);
    DrawRect(FLinearColor(.015f, .025f, .045f, .88f), 16, 16, W, 320);
    DrawText(TEXT("POWDER / TUBE PASSAGE"), FLinearColor(.2f, .8f, 1.f), 30, 26, nullptr, 1.5f);
    const FString State = UGameplayStatics::IsGamePaused(this) ? TEXT("PAUSED") :
        (!Test->bReleased ? TEXT("READY - release in 1.5 seconds") : (Test->MovingCount > 0 ? TEXT("FALLING / CONTACT") : TEXT("SETTLED")));
    DrawText(State, FLinearColor::White, 30, 57, nullptr, 1.1f);
    DrawText(FString::Printf(TEXT("300 particles | moving %d | t %.2f s | g 9.8 m/s2"), Test->MovingCount, FMath::Max(0.f, Test->Elapsed - 1.5f)), FLinearColor::White, 30, 81);
    DrawText(TEXT("R  Restart     SPACE  Pause / Resume     SHIFT+ESC  Stop"), FLinearColor(.9f, .85f, .45f), 30, 105);
    DrawText(TEXT("Enlarged powder proxies (6 cm). Gravity + contact only."), FLinearColor(.65f, .72f, .8f), 30, 130);
    DrawText(TEXT("RMB + mouse: Look | RMB + WASD / Q,E: Fly / Down,Up"), FLinearColor::White, 30, 154);
    DrawText(TEXT("MMB: Pan | Wheel: Dolly | RMB + Wheel: Speed"), FLinearColor::White, 30, 175);
    const auto* PC = Cast<APowderCameraController>(GetOwningPlayerController());
    DrawText(FString::Printf(TEXT("Shift: Boost | F: Focus | Home: Reset view | %.0f cm/s"), PC ? PC->GetNavigationSpeed() : 0.f), FLinearColor(.65f, .8f, 1.f), 30, 196);
    DrawText(FString::Printf(TEXT("Fixture contacts %d | touched parts %d"), Test->ObstacleContacts, Test->TouchedObstacleCount), FLinearColor(.4f, 1.f, .6f), 30, 220);
    DrawText(FString::Printf(TEXT("Multi-contact particles %d / 300 | Feed > Tube > Bin"), Test->MultiObstacleParticleCount), FLinearColor(.4f, 1.f, .6f), 30, 241);
    const FString PassageRate = Test->TubeEntered ? FString::Printf(TEXT("%.1f%%"), 100.f * Test->TubePassed / Test->TubeEntered) : TEXT("--");
    DrawText(FString::Printf(TEXT("Tube entered %d | passed %d | rate %s"), Test->TubeEntered, Test->TubePassed, *PassageRate), FLinearColor(.3f, .9f, 1.f), 30, 265);
    DrawText(FString::Printf(TEXT("Inside now %d | returned upstream %d | ID 24 cm"), Test->TubeInside, Test->TubeReturned), FLinearColor(.3f, .9f, 1.f), 30, 286);
    DrawText(TEXT("T: Tube close-up | Cutaway display; all walls collide"), FLinearColor(.9f, .85f, .45f), 30, 307);
    if (Test->EscapedCount > 0) DrawText(TEXT("WARNING: particle outside test platform"), FLinearColor::Red, 30, 350);
}

APowderDropGameMode::APowderDropGameMode()
{
    DefaultPawnClass = nullptr;
    PlayerControllerClass = APowderCameraController::StaticClass();
    HUDClass = APowderDropHUD::StaticClass();
}

void APowderDropGameMode::StartPlay()
{
    Super::StartPlay();
    for (TActorIterator<APowderDropTest> It(GetWorld()); It; ++It) return;
    GetWorld()->SpawnActor<APowderDropTest>();
}

void APowderDropTest::BuildObstacleCourse()
{
    ObstacleMaterial = NewObject<UPhysicalMaterial>(this);
    ObstacleMaterial->Friction = .22f;
    ObstacleMaterial->Restitution = .15f;
    ObstacleMaterial->bOverrideFrictionCombineMode = true;
    ObstacleMaterial->FrictionCombineMode = EFrictionCombineMode::Min;
    for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
    {
        if (!It->ActorHasTag(TEXT("PowderRig"))) continue;
        auto* Mesh = It->GetStaticMeshComponent();
        Mesh->SetPhysMaterialOverride(ObstacleMaterial);
        if (It->ActorHasTag(TEXT("PowderCutaway"))) Mesh->SetVisibility(false);
        Obstacles.Add(*It);
    }
    UE_LOG(LogTemp, Display, TEXT("PowderRig CHECK: saved fixture actors=%d"), Obstacles.Num());
}

void APowderDropTest::RecordObstacleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
    if (!bReleased || !OtherActor) return;
    if (OtherActor->ActorHasTag(TEXT("PowderTube"))) { ++TubeWallContacts; return; }
    if (!OtherActor->ActorHasTag(TEXT("PowderRig"))) return;
    ++ObstacleContacts;
    if (!TouchedObstacles.Contains(OtherActor))
    {
        TouchedObstacles.Add(OtherActor);
        TouchedObstacleCount = TouchedObstacles.Num();
        UE_LOG(LogTemp, Display, TEXT("PowderCourse FIRST HIT: %s at t=%.2f"), *OtherActor->GetName(), Elapsed);
    }
    auto& History = ParticleObstacleHistory.FindOrAdd(HitComponent);
    const int32 Before = History.Num();
    History.Add(OtherActor);
    if (Before == 1 && History.Num() == 2) ++MultiObstacleParticleCount;
}
bool APowderDropTest::IsWithinTubeBore(const FVector& Position) const
{
    const FVector D = Position - TubeCenterXY();
    // Sphere-aware clearance from all 16 inner wall planes; not a bounding-box overlap.
    for (int32 I = 0; I < 16; ++I)
    {
        const float A = 2.f * PI * I / 16;
        if (D.X * FMath::Cos(A) + D.Y * FMath::Sin(A) > TubeInnerRadius - RadiusCm + .15f) return false;
    }
    return true;
}

void APowderDropTest::UpdateTubeTracking()
{
    TubeInside = 0;
    if (!bReleased) return;
    for (int32 I = 0; I < Particles.Num(); ++I)
    {
        auto& State = TubeStates[I];
        const FVector Now = Particles[I]->GetComponentLocation() - GetActorLocation();
        auto Crosses = [&](float Z, bool bDown)
        {
            if (bDown ? !(State.Previous.Z > Z && Now.Z <= Z) : !(State.Previous.Z < Z && Now.Z >= Z)) return false;
            const float Alpha = (Z - State.Previous.Z) / (Now.Z - State.Previous.Z);
            return IsWithinTubeBore(FMath::Lerp(State.Previous, Now, Alpha));
        };
        // Count entry when the entire sphere has crossed inside the top opening.
        if (Crosses(TubeTop - RadiusCm, true))
        {
            if (!State.bEverEntered) { State.bEverEntered = true; ++TubeEntered; }
            State.bActive = true;
        }
        // Only a particle admitted at the top can count as a successful downstream exit.
        if (State.bActive && Crosses(TubeBottom + RadiusCm, true)) State.bExitPending = true;
        if (State.bExitPending && Now.Z > TubeBottom + RadiusCm) State.bExitPending = false;
        if (State.bActive && State.bExitPending && Now.Z <= TubeBottom - RadiusCm)
        {
            if (!State.bPassed) { State.bPassed = true; ++TubePassed; }
            State.bActive = false;
        }
        if (State.bActive && Now.Z >= TubeTop + RadiusCm)
        {
            if (!State.bReturned) { State.bReturned = true; ++TubeReturned; }
            State.bActive = false;
        }
        if (State.bActive && Now.Z >= TubeBottom - RadiusCm && Now.Z <= TubeTop + RadiusCm && IsWithinTubeBore(Now)) ++TubeInside;
        State.Previous = Now;
    }
}

void APowderDropTest::FocusTube()
{
    if (!Camera) return;
    const FVector Target = GetActorLocation() + TubeCenterXY() + FVector(0, 0, (TubeTop + TubeBottom) * .5f);
    const FVector Position = Target + FVector(130, -180, 100);
    Camera->SetActorLocation(Position);
    Camera->SetActorRotation((Target - Position).Rotation());
}