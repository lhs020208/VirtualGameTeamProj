#include "PowderCameraController.h"
#include "PowderDropTest.h"
#include "Camera/CameraActor.h"
#include "Components/InputComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerInput.h"
#include "Misc/App.h"
#include "UnrealClient.h"

APowderCameraController::APowderCameraController()
{
    bAutoManageActiveCameraTarget = false;
    bShouldPerformFullTickWhenPaused = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
}

void APowderCameraController::BeginPlay()
{
    Super::BeginPlay();
    GetWorld()->bIsCameraMoveableWhenPaused = true; // Update renderer history during paused navigation.
    UpdateCapture();
}

void APowderCameraController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &APowderCameraController::BeginLook).bExecuteWhenPaused = true;
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &APowderCameraController::EndLook).bExecuteWhenPaused = true;
    InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Pressed, this, &APowderCameraController::BeginPan).bExecuteWhenPaused = true;
    InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Released, this, &APowderCameraController::EndPan).bExecuteWhenPaused = true;
    InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &APowderCameraController::FasterOrForward).bExecuteWhenPaused = true;
    InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &APowderCameraController::SlowerOrBackward).bExecuteWhenPaused = true;
    InputComponent->BindKey(EKeys::F, IE_Pressed, this, &APowderCameraController::FocusPowder).bExecuteWhenPaused = true;
    InputComponent->BindKey(EKeys::Home, IE_Pressed, this, &APowderCameraController::RestoreView).bExecuteWhenPaused = true;
}

void APowderCameraController::BeginLook() { bLooking = true; UpdateCapture(); }
void APowderCameraController::EndLook() { bLooking = false; Velocity = FVector::ZeroVector; UpdateCapture(); }
void APowderCameraController::BeginPan() { bPanning = true; UpdateCapture(); }
void APowderCameraController::EndPan() { bPanning = false; UpdateCapture(); }

void APowderCameraController::UpdateCapture()
{
    bShowMouseCursor = !(bLooking || bPanning);
    if (bShowMouseCursor)
    {
        FInputModeGameAndUI Mode;
        Mode.SetHideCursorDuringCapture(false);
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(Mode);
    }
    else
    {
        FInputModeGameOnly Mode;
        Mode.SetConsumeCaptureMouseDown(false);
        SetInputMode(Mode);
    }
}

void APowderCameraController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    auto* Camera = Cast<ACameraActor>(GetViewTarget());
    if (!Camera || !IsLocalController()) return;
    // Losing viewport focus (Alt-Tab/Shift-F1) must not leave navigation latched.
    auto* ViewportClient = GetWorld()->GetGameViewport();
    if (!ViewportClient || !ViewportClient->Viewport || !ViewportClient->Viewport->HasFocus())
    {
        if (bLooking || bPanning) { bLooking = bPanning = false; UpdateCapture(); }
        Velocity = FVector::ZeroVector;
        return;
    }
    // Read real frame time so navigation works while Space pauses the physics world.
    const float Dt = FMath::Clamp(static_cast<float>(FApp::GetDeltaTime()), 0.f, .05f);
    // Use pixel deltas, not the project's legacy 0.07 axis sensitivity.
    const float MX = PlayerInput ? PlayerInput->GetRawKeyValue(EKeys::MouseX) : 0.f;
    const float MY = PlayerInput ? PlayerInput->GetRawKeyValue(EKeys::MouseY) : 0.f;
    if (bPanning)
    {
        Camera->AddActorWorldOffset((-Camera->GetActorRightVector() * MX - Camera->GetActorUpVector() * MY) * (MoveSpeed * .003f));
    }
    else if (bLooking)
    {
        FRotator Rotation = Camera->GetActorRotation();
        Rotation.Yaw += MX * .25f;
        Rotation.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Rotation.Pitch + MY * .25f), -89.f, 89.f);
        Rotation.Roll = 0.f;
        Camera->SetActorRotation(Rotation);
    }
    FVector Direction = FVector::ZeroVector;
    if (bLooking)
    {
        Direction += Camera->GetActorForwardVector() * (float(IsInputKeyDown(EKeys::W)) - float(IsInputKeyDown(EKeys::S)));
        Direction += Camera->GetActorRightVector() * (float(IsInputKeyDown(EKeys::D)) - float(IsInputKeyDown(EKeys::A)));
        Direction += FVector::UpVector * (float(IsInputKeyDown(EKeys::E)) - float(IsInputKeyDown(EKeys::Q)));
    }
    const float Boost = (IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift)) ? 3.f : 1.f;
    // Exponential smoothing, with diagonal normalization, keeps speed consistent across frame rates.
    const FVector TargetVelocity = Direction.GetClampedToMaxSize(1.f) * MoveSpeed * Boost;
    Velocity = FMath::Lerp(Velocity, TargetVelocity, 1.f - FMath::Exp(-14.f * Dt));
    Camera->AddActorWorldOffset(Velocity * Dt);
}

void APowderCameraController::FasterOrForward() { Scroll(1.f); }
void APowderCameraController::SlowerOrBackward() { Scroll(-1.f); }
void APowderCameraController::Scroll(float Direction)
{
    if (bLooking) MoveSpeed = FMath::Clamp(MoveSpeed * FMath::Pow(1.25f, Direction), 10.f, 4000.f);
    else if (auto* Camera = Cast<ACameraActor>(GetViewTarget()))
        Camera->AddActorWorldOffset(Camera->GetActorForwardVector() * Direction * MoveSpeed * .15f);
}

void APowderCameraController::FocusPowder()
{
    auto* Camera = Cast<ACameraActor>(GetViewTarget());
    if (!Camera) return;
    for (TActorIterator<APowderDropTest> It(GetWorld()); It; ++It)
    {
        const FVector Target = It->GetActorLocation() + FVector(0, 0, APowderDropTest::FloorTop + APowderDropTest::RadiusCm + It->MeanHeight);
        Camera->SetActorLocation(Target - Camera->GetActorForwardVector() * 220.f);
        Velocity = FVector::ZeroVector;
        return;
    }
}

void APowderCameraController::RestoreView()
{
    auto* Camera = Cast<ACameraActor>(GetViewTarget());
    if (!Camera) return;
    for (TActorIterator<APowderDropTest> It(GetWorld()); It; ++It)
    {
        const FVector Origin = It->GetActorLocation();
        const FVector Position = Origin + FVector(340, -480, 290);
        Camera->SetActorLocation(Position);
        Camera->SetActorRotation((Origin + FVector(0, 0, 105) - Position).Rotation());
        Velocity = FVector::ZeroVector;
        return;
    }
}
