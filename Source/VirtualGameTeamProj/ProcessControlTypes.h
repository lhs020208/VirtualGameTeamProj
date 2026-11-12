#pragma once

#include "CoreMinimal.h"
#include "PlantSimulationSnapshot.h"

// These limits are the plant worker's live-control limits, not the generic GPU API's limits.
namespace PlantControl { constexpr double MinRPM = 0.0, MaxRPM = 30.0; }

enum class EProcessCommand : uint8 { Valve, MixerEnabled, MixerRPM, StopAll, Resume, Pause };
enum class EProcessCommandStatus : uint8 { Accepted, Processing, Applied, Rejected, Failed, Unknown };
enum class EProcessStopStage : uint8 { None, Requested, Closing, Pausing, Complete, Failed };
enum class EProcessFreezeState : uint8 { None, Awaiting, Confirmed, Unconfirmed };
enum class EProcessTimeoutPolicy : uint8 { Unspecified, Freeze, KeepClosing };
enum class EProcessPausePolicy : uint8 { Unspecified, PreserveRequests, CloseThenPause };
enum class EProcessPausedClosePolicy : uint8 { Unspecified, StopAll, RequireStopButton };

struct FProcessCommandRecord
{
    FGuid RequestId;
    int64 Epoch = -1;
    int64 Connection = 0;
    FString EquipmentId;
    EProcessCommand Command = EProcessCommand::Valve;
    double Value = 0;
    EProcessCommandStatus Status = EProcessCommandStatus::Accepted;
    FString Message;
    double AcceptedAt = 0, Deadline = 0;
    int64 BaselineSequence = 0, Revision = 0;
    bool UsesStopSequence = false;
    bool IsPending() const { return Status == EProcessCommandStatus::Accepted || Status == EProcessCommandStatus::Processing; }
};

// A single mailbox message. The worker takes one coherent copy under its mutex.
struct FPlantControlTargets
{
    int64 Revision = 0;
    int64 ObservationConnection = 0;
    bool Running = true;
    uint32 Suppliers = 0;
    bool WeighingOpen = false, ReceiverOpen = false, MixerEnabled = false;
    double RPM = 15, Pressure = 1000;
};

// Transport is injected for service tests; production binds it exclusively to APowderPlantTest.
using FProcessControlTransport = TFunction<bool(EProcessCommand, const FString&, double, int64, int64&, FString&)>;

class FProcessControlMachine
{
public:
    // Trial wall-clock limits, configurable on the owning service. Never simulation time.
    double CommandTimeoutSeconds = 10, StopTimeoutSeconds = 30;
    EProcessTimeoutPolicy TimeoutPolicy = EProcessTimeoutPolicy::Unspecified;
    EProcessPausePolicy PausePolicy = EProcessPausePolicy::Unspecified;
    EProcessPausedClosePolicy PausedClosePolicy = EProcessPausedClosePolicy::Unspecified;
    FPlantSimulationSnapshot Snapshot;
    TMap<FGuid, FProcessCommandRecord> History;
    FGuid LastRequest, StopRequest;
    EProcessStopStage StopStage = EProcessStopStage::None;
    EProcessFreezeState FreezeState = EProcessFreezeState::None;
    FString StopFailureEquipment;
    bool ManualGranted = false, Connected = false;
    int64 Connection = 0;

    void Connect(const FPlantSimulationSnapshot& State);
    void Disconnect(const FString& Reason);
    void Observe(const FPlantSimulationSnapshot& State, double Now, const FProcessControlTransport& Send);
    FProcessCommandRecord Submit(FGuid Id, int64 Epoch, const FString& Equipment, EProcessCommand Command, double Value,
        double Now, const FProcessControlTransport& Send);
    FString UnavailableReason(const FString& Equipment, EProcessCommand Command, double Value, double Now) const;
    FString StopDescription() const;
    FString UnconfirmedEquipment() const;
    bool IsStopping() const { return StopStage == EProcessStopStage::Requested || StopStage == EProcessStopStage::Closing || StopStage == EProcessStopStage::Pausing; }
    static FString StatusText(EProcessCommandStatus Status);
private:
    int64 StopRevision = 0, PauseRevision = 0, FreezeRevision = 0, FreezeBaselineSequence = 0;
    double StopDeadline = 0, FreezeDeadline = 0;
    FString StopFailureReason;
    bool UsesStopSequence(EProcessCommand Command, double Value) const;
    bool Fresh(double Now) const;
    bool Matches(const FProcessCommandRecord& R) const;
    bool AllClosed() const;
    void InvalidatePending(const FString& Reason, EProcessCommandStatus Status);
    void FailStop(const FString& Reason, double Now, const FProcessControlTransport& Send);
    void SetFreezeResult(EProcessFreezeState Result, const FString& Message);
};
