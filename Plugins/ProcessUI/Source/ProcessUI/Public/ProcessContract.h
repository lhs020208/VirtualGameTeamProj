#pragma once
#include "CoreMinimal.h"

namespace ProcessUI
{
enum class EValidity : uint8 { Valid, Unsupported, Missing, Stale, Error };
struct FReading
{
    double Value = 0; // Never display or consume unless Valid.
    EValidity Validity = EValidity::Unsupported;
    double SampleTime = 0;
    FString Unit;
    static FReading Known(double V, const TCHAR* U, double T) { return {V, EValidity::Valid, T, U}; }
    bool Valid() const { return Validity == EValidity::Valid && FMath::IsFinite(Value); }
};
enum class EStepKind : uint8 { WeighTransfer, MixerCharge };
enum class ETiming : uint8 { Initial, AfterMixing };
enum class ERun : uint8 { Idle, Running, Paused, Aborted, Complete };
enum class EOperation : uint8 { None, Feed, Settle, Transfer, WaitCharge, Charge, Mixing, Stopping };
enum class ECommandKind : uint8 { Feed, Transfer, Charge, Mix, StopAll, CloseValve, OpenValve, Reset };
enum class ECommandResult : uint8 { Accepted, Applied, Rejected, Failed };
struct FHopper
{
    FString Id, Material;
    bool Additive = false;
    double DiameterM = 0;
    double RateKgS = 0;
};
struct FStep
{
    FString Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    FString Name;
    EStepKind Kind = EStepKind::WeighTransfer;
    FString HopperId;
    double TargetKg = 0;
    ETiming Timing = ETiming::Initial;
    double AtSeconds = 0;
};
struct FRecipe
{
    int32 Version = 1;
    FString Name;
    TArray<FHopper> Hoppers;
    TArray<FStep> Steps;
    double RPM = 0, MixSeconds = 0;
};
struct FDevice
{
    FString Id, Label, ValveId;
    FReading Mass, TotalIn, TotalOut, ValveClosed, Stopped, Temperature, ParticleCount;
    FReading AppliedRate, ActualRate, AppliedDiameter;
    bool MaterialsValid = false;
    TMap<FString,double> MaterialKg;
    FString Warning, Error;
};
struct FCommand
{
    uint64 Request = 0, Generation = 0;
    FString Batch, Step, Device;
    ECommandKind Kind = ECommandKind::StopAll;
    double Target = 0, Rate = 0, DiameterM = 0;
    FString Material;
};
struct FReceipt
{
    uint64 Request = 0, Generation = 0;
    ECommandResult Result = ECommandResult::Accepted;
    FString Device, Detail;
    FString Step;
    FReading AppliedTarget;
};
struct FSnapshot
{
    FString Source;
    uint64 Generation = 0, Sequence = 0;
    double SourceSeconds = 0, ReceivedSeconds = 0;
    bool TestMode = false, CanAutomate = false;
    TSet<FString> Capabilities;
    uint64 ConditionsRequest = 0;
    uint64 StopConfirmedRequest = 0;
    EValidity ConditionsValidity = EValidity::Unsupported;
    bool SupplyEnded = false, Stable = false, DischargeComplete = false, PipeClear = false;
    bool WrongMaterial = false, LedgerValid = false;
    FReading ActualRPM, AppliedRPM;
    TArray<FDevice> Devices;
    TMap<FString, double> Weighed, Transferred, Charged;
    TMap<FString, double> ChargeStartedAt;
    TArray<FReceipt> Receipts;
    FString Error;
};
struct FBounds
{
    bool Valid = false;
    double WeighCapacityKg = 0, ReceiverCapacityKg = 0, MixerCapacityKg = 0;
    double TransferKgS = 0, SettleSeconds = 0, CommandSeconds = 0;
};
class PROCESSUI_API IProvider
{
public:
    virtual ~IProvider() = default;
    virtual void Tick(double Seconds) = 0;
    virtual const FSnapshot& Snapshot() const = 0;
    virtual FBounds Bounds() const = 0;
    virtual void Submit(const FCommand& Command) = 0;
};
PROCESSUI_API FString Display(const FReading& Reading, double Scale = 1, const FString& Unit = TEXT(""));
PROCESSUI_API FRecipe DemoRecipe();
PROCESSUI_API bool SaveRecipe(const FRecipe&, const FString& Directory, FString& Error);
PROCESSUI_API bool LoadRecipe(const FString& Name, const FString& Directory, FRecipe&, FString& Error);
}
