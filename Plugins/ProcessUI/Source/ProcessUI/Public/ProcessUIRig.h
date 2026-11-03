#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProcessController.h"
#include "ProcessTestProvider.h"
#include "ProcessUIRig.generated.h"
class SVerticalBox;
class SWidget;
class ACameraActor;
UCLASS()
class PROCESSUI_API AProcessUIRig : public AActor
{
    GENERATED_BODY()
public:
    AProcessUIRig();
    ~AProcessUIRig();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    /** Assign an existing PowderPlantTest instance after team merge. Read-only; no toggle/reset writes. */
    UPROPERTY(EditAnywhere, Category="Process UI|Integration") TObjectPtr<AActor> DEMSource;
    UPROPERTY(EditAnywhere, Category="Process UI|Integration") bool bObserveDEM = false;
    /** Explicit equipment camera targets; never infer equipment identity from actor order. */
    UPROPERTY(EditAnywhere, Category="Process UI|Integration") TMap<FName,TObjectPtr<AActor>> EquipmentFocusTargets;
    UPROPERTY(EditAnywhere, Category="Process UI") bool bShowPanel = true;
    ProcessUI::FController& Control() { return *Controller; }
    void SetRecipe(const ProcessUI::FRecipe& Recipe);
private:
    TSharedPtr<ProcessUI::FController> Controller;
    TSharedPtr<ProcessUI::FTestProvider> TestProvider;
    TSharedPtr<SWidget> Panel;
    TSharedPtr<SVerticalBox> Body;
    TSharedPtr<SVerticalBox> Legend;
    UPROPERTY() TObjectPtr<ACameraActor> Camera;
    UPROPERTY() TArray<TObjectPtr<USceneComponent>> Visuals;
    TMap<FString,FVector> Positions;
    int32 Tab = 0;
    bool bGrams = true, bMicrometers = false;
    double VisualTime = 0;
    bool bSmoke = false, bSmokeShot = false;
    int32 SmokeStage = 0;
    FString Notice;
    FString SaveName = TEXT("내 공정 설정");
    TSharedRef<SWidget> BuildPanel();
    void RebuildBody();
    void BuildVisuals();
    void Focus(const FString& Id);
    FString DeviceText() const;
    FString Summary() const;
};
