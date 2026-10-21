#include "PowderDropTest.h"
#include "PowderCameraController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"
#include "InputCoreTypes.h"

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
    Platform->SetRelativeScale3D(FVector(2.4, 2.4, .04));
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
    FRandomStream Random(100);
    for (int32 I = 0; I < ParticleCount; ++I)
    {
        // 5 x 5 x 4; 8 cm spacing exceeds the 6 cm diameter. No initial overlaps.
        const FVector P((I % 5 - 2) * 8.f + Random.FRandRange(-.4f, .4f),
            ((I / 5) % 5 - 2) * 8.f + Random.FRandRange(-.4f, .4f),
            212.f + (I / 25) * 9.f);
        InitialTransforms.Add(FTransform(FQuat::Identity, GetActorLocation() + P, FVector(RadiusCm / 50.f)));
        Particles[I]->SetPhysMaterialOverride(ContactMaterial);
        Particles[I]->SetMassOverrideInKg(NAME_None, .01f, true);
    }
    Camera = GetWorld()->SpawnActor<ACameraActor>();
    const FVector CameraPosition = GetActorLocation() + FVector(340, -480, 290);
    Camera->SetActorLocation(CameraPosition);
    Camera->SetActorRotation((GetActorLocation() + FVector(0, 0, 105) - CameraPosition).Rotation());
    Camera->GetCameraComponent()->SetFieldOfView(55.f);
    ResetDrop();
}

void APowderDropTest::ResetDrop()
{
    if (auto* PC = GetWorld()->GetFirstPlayerController()) PC->SetPause(false);
    Elapsed = 0.f;
    NextReport = 0.f;
    bReleased = false;
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
        UE_LOG(LogTemp, Display, TEXT("PowderDrop RELEASE: 100 particles, zero initial velocity"));
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
        if (Position.ContainsNaN() || Position.Z < FloorTop - RadiusCm || FMath::Abs(Position.X) > 120.f || FMath::Abs(Position.Y) > 120.f) ++EscapedCount;
    }
    MeanHeight /= ParticleCount;
    if (Elapsed >= NextReport)
    {
        UE_LOG(LogTemp, Display, TEXT("PowderDrop CHECK: t=%.2f count=%d moving=%d escaped=%d meanClearance=%.2fcm minCenterZ=%.2fcm maxSpeed=%.2fcm/s"),
            Elapsed, Particles.Num(), MovingCount, EscapedCount, MeanHeight, MinZ, MaxSpeed);
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
    DrawRect(FLinearColor(.015f, .025f, .045f, .88f), 16, 16, W, 212);
    DrawText(TEXT("POWDER / FREE FALL"), FLinearColor(.2f, .8f, 1.f), 30, 26, nullptr, 1.5f);
    const FString State = UGameplayStatics::IsGamePaused(this) ? TEXT("PAUSED") :
        (!Test->bReleased ? TEXT("READY - release in 1.5 seconds") : (Test->MovingCount > 0 ? TEXT("FALLING / CONTACT") : TEXT("SETTLED")));
    DrawText(State, FLinearColor::White, 30, 57, nullptr, 1.1f);
    DrawText(FString::Printf(TEXT("100 particles | moving %d | t %.2f s | g 9.8 m/s2"), Test->MovingCount, FMath::Max(0.f, Test->Elapsed - 1.5f)), FLinearColor::White, 30, 81);
    DrawText(TEXT("R  Restart     SPACE  Pause / Resume     SHIFT+ESC  Stop"), FLinearColor(.9f, .85f, .45f), 30, 105);
    DrawText(TEXT("Enlarged powder proxies (6 cm). Gravity + contact only."), FLinearColor(.65f, .72f, .8f), 30, 130);
    DrawText(TEXT("RMB + mouse: Look | RMB + WASD / Q,E: Fly / Down,Up"), FLinearColor::White, 30, 154);
    DrawText(TEXT("MMB: Pan | Wheel: Dolly | RMB + Wheel: Speed"), FLinearColor::White, 30, 175);
    const auto* PC = Cast<APowderCameraController>(GetOwningPlayerController());
    DrawText(FString::Printf(TEXT("Shift: Boost | F: Focus | Home: Reset view | %.0f cm/s"), PC ? PC->GetNavigationSpeed() : 0.f), FLinearColor(.65f, .8f, 1.f), 30, 196);
    if (Test->EscapedCount > 0) DrawText(TEXT("WARNING: particle outside test platform"), FLinearColor::Red, 30, 235);
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
