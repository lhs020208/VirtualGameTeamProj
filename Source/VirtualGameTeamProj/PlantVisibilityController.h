#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlantVisibilityController.generated.h"

class SWidget;
class SWindow;
class ACameraActor;

UENUM(BlueprintType)
enum class EPlantViewMode : uint8
{
    Full UMETA(DisplayName="전체"),
    Cutaway UMETA(DisplayName="전면 개방"),
    Hidden UMETA(DisplayName="설비 숨김")
};

/** Explicit part list: never discovers/hides particles, lights or unrelated actors. */
USTRUCT(BlueprintType)
struct FPlantVisualPart
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<AActor> Actor = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bFrame = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCutaway = false;
    // The clipping plane follows this part's transform. Unreal centimeters.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector LocalPlaneOrigin = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector LocalPlaneNormal = FVector::YAxisVector;
};

/** Display-only controls. No solver, collision, transform or physics state writes. */
UCLASS()
class VIRTUALGAMETEAMPROJ_API APlantVisibilityController : public AActor
{
    GENERATED_BODY()
public:
    APlantVisibilityController();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plant Display") EPlantViewMode ViewMode = EPlantViewMode::Full;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plant Display") bool bShowFrames = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plant Display") bool bShowRuntimePanel = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plant Display") TArray<FPlantVisualPart> Parts;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plant Display") TObjectPtr<ACameraActor> OverviewCamera;

    UFUNCTION(BlueprintCallable, Category="Plant Display") void SetViewMode(EPlantViewMode Mode);
    UFUNCTION(BlueprintCallable, Category="Plant Display") void SetFramesVisible(bool bVisible);
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Plant Display", meta=(DisplayName="표시 패널 열기")) void OpenDisplayPanel();
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Plant Display", meta=(DisplayName="전체 보기")) void ShowFull();
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Plant Display", meta=(DisplayName="전면 개방")) void ShowCutaway();
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Plant Display", meta=(DisplayName="설비 숨기기")) void HideEquipment();
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Plant Display", meta=(DisplayName="프레임 전환")) void ToggleFrames();
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Plant Display", meta=(DisplayName="표시 적용")) void ApplyDisplay();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif
private:
    TSharedRef<SWidget> BuildPanel();
    TSharedPtr<SWidget> ViewportPanel;
    TWeakPtr<SWindow> EditorPanel;
};
