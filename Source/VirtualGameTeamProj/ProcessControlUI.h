#pragma once
#include "CoreMinimal.h"
#include "ProcessControlTypes.h"
#include "ProcessControlUI.generated.h"
class UUserWidget;
class UProcessControlService;
class UHorizontalBox;
class UVerticalBox;

/** Runtime-only bindings to the existing widget; no asset graph or physics writes. */
UCLASS()
class UProcessControlUI : public UObject
{
    GENERATED_BODY()
public:
    bool Attach(UUserWidget* Widget, UProcessControlService* Service);
    void Detach();
    void Refresh();
    UFUNCTION() void OpenValve();
    UFUNCTION() void CloseValve();
    UFUNCTION() void StartMixer();
    UFUNCTION() void StopMixer();
    UFUNCTION() void SetRPM();
    UFUNCTION() void ApplyPressure();
    UFUNCTION() void ApplyPurgePressure();
    UFUNCTION() void StartPurge();
    UFUNCTION() void StopPurge();
    UFUNCTION() void StopAll();
    UFUNCTION() void Resume();
    UFUNCTION() void Pause();
private:
    TWeakObjectPtr<UUserWidget> Widget;
    TWeakObjectPtr<UProcessControlService> Service;
    UPROPERTY(Transient) TObjectPtr<UHorizontalBox> GlobalControls;
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
