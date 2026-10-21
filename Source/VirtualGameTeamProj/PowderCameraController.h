#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PowderCameraController.generated.h"

/** Runtime viewport navigation, independent of the powder simulation clock. */
UCLASS()
class VIRTUALGAMETEAMPROJ_API APowderCameraController : public APlayerController
{
    GENERATED_BODY()
public:
    APowderCameraController();
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float DeltaTime) override;
    float GetNavigationSpeed() const { return MoveSpeed; }
private:
    void BeginLook();
    void EndLook();
    void BeginPan();
    void EndPan();
    void UpdateCapture();
    void FasterOrForward();
    void SlowerOrBackward();
    void Scroll(float Direction);
    void FocusPowder();
    void RestoreView();
    bool bLooking = false;
    bool bPanning = false;
    float MoveSpeed = 200.f;
    FVector Velocity = FVector::ZeroVector;
};
