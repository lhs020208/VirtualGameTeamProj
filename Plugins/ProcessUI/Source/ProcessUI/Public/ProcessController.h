#pragma once
#include "ProcessContract.h"

namespace ProcessUI
{
/** Owns sequencing, exception handling and receiver ownership. Widgets only issue intents. */
class PROCESSUI_API FController
{
public:
    explicit FController(TSharedRef<IProvider> InProvider) : Provider(InProvider) { Generation=Provider->Snapshot().Generation; }
    FRecipe Recipe;
    ERun Run = ERun::Idle;
    EOperation Operation = EOperation::None;
    int32 StepIndex = 0;
    double MixingSeconds = 0;
    FString Message, Batch, ReceiverOwner;
    FString ExceptionReason;
    bool MustReset = false;
    TArray<FString> Validate(const FRecipe& Candidate) const;
    bool Start();
    void Tick(double Seconds);
    void Pause(const FString& Reason);
    bool Resume();
    void Abort();
    bool Reset();
    void ManualValve(const FString& ValveId, bool Open);
    const FSnapshot& Snapshot() const { return Provider->Snapshot(); }
    bool ConfirmedSafe() const;
    bool Editable() const { return Run == ERun::Idle; }
    FString StatusText() const;
    FString WaitingText() const;
private:
    TSharedRef<IProvider> Provider;
    uint64 NextRequest = 1, Generation = 0, LastSequence = 0;
    double LastSourceTime = 0, NoUpdateSeconds = 0;
    uint64 ObservedSequence = 0;
    bool MixingStarted = false, AwaitMix = false, ResetPending = false;
    EOperation SuspendedOperation = EOperation::None;
    TMap<FString,double> PauseWeighed, PauseTransferred, PauseCharged;
    double DueMissed = -1;
    uint64 ActiveRequest = 0;
    uint64 LastStopRequest = 0, LastStopGeneration = 0;
    TSet<FString> StartedCharges;
    bool CommandApplied() const;
    bool ConditionsCurrent() const;
    void Send(ECommandKind Kind, const FString& Device = TEXT(""), double Target = 0, const FString& Step = TEXT(""));
    void EnterStep();
    void CaptureLedger();
    bool LedgerConsistent() const;
    int32 NextCharge() const;
};
}
