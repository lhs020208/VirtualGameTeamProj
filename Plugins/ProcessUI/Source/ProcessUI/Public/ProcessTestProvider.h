#pragma once
#include "ProcessContract.h"

namespace ProcessUI
{
/** Deterministic contract simulator. No DEM, CFD or equipment sizing claims. */
class PROCESSUI_API FTestProvider : public IProvider
{
public:
    explicit FTestProvider(const TArray<FHopper>& Hoppers);
    void Tick(double Seconds) override;
    const FSnapshot& Snapshot() const override { return State; }
    FBounds Bounds() const override { return {true, 5, 10, 20, 0.05, 0.3, 0.1}; }
    void Submit(const FCommand& Command) override;
    double PreparationSpeed = 1;
    FString FailCloseValve;
    void InjectWrongMaterial() { State.WrongMaterial = true; }
private:
    FSnapshot State;
    FCommand Active;
    FCommand StopCommand;
    bool HasActive = false, Stopping = false;
    double Delay = 0, StopDelay = 0, RequestedRPM = 0, FinishWait = -1;
    TMap<uint64,FReceipt> Seen;
    TMap<FString,FString> StepHopper;
    void Receipt(const FCommand&, ECommandResult, const FString&);
    FDevice* Device(const FString& Id);
    void SetReading(FReading& R, double V, const TCHAR* Unit);
};
}
