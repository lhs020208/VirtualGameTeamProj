#include "PlantVisibilityController.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"

APlantVisibilityController::APlantVisibilityController()
{
    PrimaryActorTick.bCanEverTick = false;
}
void APlantVisibilityController::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyDisplay();
}
void APlantVisibilityController::SetViewMode(EPlantViewMode Mode)
{
#if WITH_EDITOR
    if (GetWorld() && !GetWorld()->IsGameWorld()) Modify();
#endif
    ViewMode = Mode;
    ApplyDisplay();
}
void APlantVisibilityController::SetFramesVisible(bool bVisible)
{
#if WITH_EDITOR
    if (GetWorld() && !GetWorld()->IsGameWorld()) Modify();
#endif
    bShowFrames = bVisible;
    ApplyDisplay();
}
void APlantVisibilityController::ShowFull() { SetViewMode(EPlantViewMode::Full); }
void APlantVisibilityController::ShowCutaway() { SetViewMode(EPlantViewMode::Cutaway); }
void APlantVisibilityController::HideEquipment() { SetViewMode(EPlantViewMode::Hidden); }
void APlantVisibilityController::ToggleFrames() { SetFramesVisible(!bShowFrames); }

void APlantVisibilityController::ApplyDisplay()
{
    for (const FPlantVisualPart& Part : Parts)
    {
        if (!IsValid(Part.Actor) || Part.Actor->GetWorld() != GetWorld()) continue;
        TInlineComponentArray<UStaticMeshComponent*> Components;
        Part.Actor->GetComponents(Components);
        const bool bVisible = Part.bFrame ? bShowFrames : ViewMode != EPlantViewMode::Hidden;
        for (UStaticMeshComponent* Component : Components)
        {
            // No propagation: imported parent/child hierarchy must not hide internal parts.
            Component->SetVisibility(bVisible, false);
            if (!Part.bCutaway) continue;
            const FTransform Transform = Component->GetComponentTransform();
            const FVector Scale = Transform.GetScale3D();
            FVector Normal = Part.LocalPlaneNormal;
            for (int32 Axis=0; Axis<3; ++Axis)
                Normal[Axis] = FMath::Abs(Scale[Axis]) > SMALL_NUMBER ? Normal[Axis]/Scale[Axis] : 0.0;
            Normal = Transform.GetRotation().RotateVector(Normal).GetSafeNormal(SMALL_NUMBER, FVector::YAxisVector);
            const FVector Origin = Transform.TransformPosition(Part.LocalPlaneOrigin);
            // CPD0=enable, CPD1..3=world normal, CPD4=world plane distance.
            Component->SetCustomPrimitiveDataVector4(1, FVector4(Normal, FVector::DotProduct(Normal, Origin)));
            Component->SetCustomPrimitiveDataFloat(0, ViewMode == EPlantViewMode::Cutaway ? 1.f : 0.f);
#if WITH_EDITOR
            if (!GetWorld()->IsGameWorld())
            {
                Component->SetDefaultCustomPrimitiveDataVector4(1, FVector4(Normal, FVector::DotProduct(Normal, Origin)));
                Component->SetDefaultCustomPrimitiveDataFloat(0, ViewMode == EPlantViewMode::Cutaway ? 1.f : 0.f);
            }
#endif
        }
    }
}

TSharedRef<SWidget> APlantVisibilityController::BuildPanel()
{
    const TWeakObjectPtr<APlantVisibilityController> WeakThis(this);
    auto Button = [WeakThis](const TCHAR* Label, EPlantViewMode Mode) -> TSharedRef<SWidget>
    {
        return SNew(SButton).IsFocusable(false).ContentPadding(FMargin(12,8))
            .OnClicked_Lambda([WeakThis, Mode]() { if (WeakThis.IsValid()) WeakThis->SetViewMode(Mode); return FReply::Handled(); })
            [SNew(STextBlock).Text(FText::FromString(Label))];
    };
    return SNew(SBorder).Padding(12).BorderBackgroundColor(FLinearColor(.035f,.045f,.06f,.96f))
        [SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
            [SNew(STextBlock).Text(FText::FromString(TEXT("공정 설비 표시 · 물리 계산과 독립")))]
        + SVerticalBox::Slot().AutoHeight()
            [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()[Button(TEXT("전체"), EPlantViewMode::Full)]
            + SHorizontalBox::Slot().AutoWidth().Padding(4,0)[Button(TEXT("전면 개방"), EPlantViewMode::Cutaway)]
            + SHorizontalBox::Slot().AutoWidth()[Button(TEXT("설비 숨김"), EPlantViewMode::Hidden)]]
        + SVerticalBox::Slot().AutoHeight().Padding(0,10,0,6)
            [SNew(SCheckBox).IsFocusable(false)
            .IsChecked_Lambda([WeakThis]() { return WeakThis.IsValid() && WeakThis->bShowFrames ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
            .OnCheckStateChanged_Lambda([WeakThis](ECheckBoxState State) { if (WeakThis.IsValid()) WeakThis->SetFramesVisible(State == ECheckBoxState::Checked); })
            [SNew(STextBlock).Text(FText::FromString(TEXT("프레임 표시 (모드와 독립)")))]]
        + SVerticalBox::Slot().AutoHeight()
            [SNew(STextBlock).Text_Lambda([WeakThis]()
            {
                if (!WeakThis.IsValid()) return FText::FromString(TEXT("레벨을 닫았습니다"));
                const TCHAR* Mode = WeakThis->ViewMode == EPlantViewMode::Full ? TEXT("전체") :
                    WeakThis->ViewMode == EPlantViewMode::Cutaway ? TEXT("전면 개방") : TEXT("숨김");
                return FText::FromString(FString::Printf(TEXT("현재: %s | 프레임: %s"),Mode,WeakThis->bShowFrames?TEXT("표시"):TEXT("숨김")));
            })]];
}

void APlantVisibilityController::OpenDisplayPanel()
{
    if (!FSlateApplication::IsInitialized()) return;
    if (GetWorld() && GetWorld()->IsGameWorld())
    {
        if (!ViewportPanel && GetWorld()->GetGameViewport())
        {
            ViewportPanel = SNew(SOverlay)
                + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(16)[BuildPanel()];
            GetWorld()->GetGameViewport()->AddViewportWidgetContent(ViewportPanel.ToSharedRef(), 30);
        }
        return;
    }
#if WITH_EDITOR
    if (auto Existing = EditorPanel.Pin()) { Existing->BringToFront(); return; }
    TSharedRef<SWindow> Window = SNew(SWindow).Title(FText::FromString(TEXT("공정 설비 표시")))
        .ClientSize(FVector2D(400,180)).SupportsMaximize(false).SupportsMinimize(false)[BuildPanel()];
    EditorPanel = Window;
    FSlateApplication::Get().AddWindow(Window);
#endif
}
void APlantVisibilityController::BeginPlay()
{
    Super::BeginPlay();
    ApplyDisplay();
    if (OverviewCamera && GetWorld()->GetFirstPlayerController())
        GetWorld()->GetFirstPlayerController()->SetViewTarget(OverviewCamera);
    if (bShowRuntimePanel) OpenDisplayPanel();
}
void APlantVisibilityController::EndPlay(const EEndPlayReason::Type Reason)
{
    if (ViewportPanel && GetWorld() && GetWorld()->GetGameViewport())
        GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(ViewportPanel.ToSharedRef());
    ViewportPanel.Reset();
    Super::EndPlay(Reason);
}
#if WITH_EDITOR
void APlantVisibilityController::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    Super::PostEditChangeProperty(Event);
    ApplyDisplay();
}
#endif
