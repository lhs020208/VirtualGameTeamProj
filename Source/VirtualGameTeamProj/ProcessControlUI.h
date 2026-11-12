#pragma once
#include "CoreMinimal.h"
#include "ProcessControlTypes.h"
#include "ProcessControlUI.generated.h"
class UUserWidget;
class UProcessControlService;
class UHorizontalBox;

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
    UFUNCTION() void StopAll();
    UFUNCTION() void Resume();
    UFUNCTION() void Pause();
private:
    TWeakObjectPtr<UUserWidget> Widget;
    TWeakObjectPtr<UProcessControlService> Service;
    UPROPERTY(Transient) TObjectPtr<UHorizontalBox> GlobalControls;
    FString LocalError;
    FString Selected() const;
    bool Is3D() const;
    void Send(EProcessCommand Command, double Value, bool Global = false);
};
