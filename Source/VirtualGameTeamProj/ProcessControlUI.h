#pragma once
#include "CoreMinimal.h"
#include "ProcessControlTypes.h"
#include "ProcessRecipe.h"
#include "Types/SlateEnums.h"
#include "ProcessControlUI.generated.h"
class UUserWidget;
class UProcessControlService;
class UHorizontalBox;
class UVerticalBox;
class UCanvasPanel;
class UBorder;

/** Runtime-only bindings to the existing widget; no asset graph or physics writes. */
UCLASS()
class UProcessControlUI : public UObject
{
    GENERATED_BODY()
public:
    bool Attach(UUserWidget* Widget, UProcessControlService* Service);
    void Detach();
    void Refresh();
    UFUNCTION() void ToggleRecipes();
    UFUNCTION() void CloseRecipes();
    UFUNCTION() void LoadSelectedRecipe();
    UFUNCTION() void SaveNewRecipe();
    UFUNCTION() void OverwriteSelectedRecipe();
    UFUNCTION() void ApplyRecipeDraft();
    UFUNCTION() void CopyAppliedRecipe();
    UFUNCTION() void RecipeModeChanged(FString Value,ESelectInfo::Type Type);
    UFUNCTION() void RecipeSelectionChanged(FString Value,ESelectInfo::Type Type);
    UFUNCTION() void RecipeTextChanged(const FText& Value);
    UFUNCTION() void RecipeCheckChanged(bool Value);
    UFUNCTION() void OpenValve();
    UFUNCTION() void CloseValve();
    UFUNCTION() void StartMixer();
    UFUNCTION() void StopMixer();
    UFUNCTION() void SetRPM();
    UFUNCTION() void ApplyPressure();
    UFUNCTION() void ApplyPurgePressure();
    UFUNCTION() void StartPurge();
    UFUNCTION() void StopPurge();
    UFUNCTION() void StartAutomaticA();
    UFUNCTION() void CheckBatchRecovery();
    UFUNCTION() void ResumeBatchRecovery();
    UFUNCTION() void AbandonBatchRecovery();
    UFUNCTION() void ApplyAutomaticMixTime();
    UFUNCTION() void SelectFullBatch();
    UFUNCTION() void SelectTargetBatch();
    UFUNCTION() void ApplyTargetMass();
    UFUNCTION() void SelectSequentialBatch();
    UFUNCTION() void ApplyBTargetMass();
    UFUNCTION() void ApplyAdditiveSettings();
    void RefreshAutomatic();
    UFUNCTION() void StopAll();
    UFUNCTION() void Resume();
    UFUNCTION() void Pause();
private:
    UPROPERTY(Transient) TObjectPtr<UBorder> RecipePanel;
    bool RecipeOpen=false,RecipeFilling=false,RecipeDirty=true;
    FString RecipeMessage;
    TArray<FProcessRecipeFile> RecipeFiles;
    void BuildRecipeControls(UCanvasPanel* Root);
    void RefreshRecipeControls();
    void RefreshRecipeList(const FString& SelectedFile=FString());
    bool ReadRecipeDraft(FProcessRecipe& Out,FString& Error) const;
    void FillRecipeDraft(const FProcessRecipe& R);
    bool RecipeEditable();
    TWeakObjectPtr<UUserWidget> Widget;
    TWeakObjectPtr<UProcessControlService> Service;
    UPROPERTY(Transient) TObjectPtr<UHorizontalBox> GlobalControls;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> MixTimeControls;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> DoseControls;
    FString DoseMessage,AdditiveMessage;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> AdditiveControls;
    void BuildAdditiveControls(UVerticalBox* Panel);
    void RefreshAdditiveControls();
    void BuildDoseControls(UVerticalBox* Panel);
    void RefreshDoseControls();
    FString MixTimeMessage;
    void BuildMixTimeControls(UVerticalBox* Panel);
    void RefreshMixTimeControls();
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> PressureControls;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> PurgeControls;
    FString PurgeDraftEquipment;
    int64 PurgeDraftEpoch=-1,PurgeDraftConnection=-1;
    FGuid PurgeDraftStop;
    void BuildPurgeControls(UVerticalBox* Panel);
    void RefreshPurgeControls();
    FString PressureDraftEquipment;
    int64 PressureDraftEpoch = -1, PressureDraftConnection = -1;
    FGuid PressureDraftStop;
    void BuildPressureControls(UVerticalBox* Panel);
    void RefreshPressureControls();
    FString LocalError;
    FString Selected() const;
    bool Is3D() const;
    void Send(EProcessCommand Command, double Value, bool Global = false);
};
