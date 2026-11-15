#pragma once
#include "PlantFeedPreparation.h"
#include "PlantMixerGeometry.h"

// Stable ID membership comes from the original charge's supplier column, never particle type/colour.
class FPlantSupplyABatch
{
public:
    FPlantSupplyABatch(const powder::GranularConfig& Config, const TArray<int32>& SupplierById,
        const FPlantTransferTelemetry& Mass, const FPlantFeedPreparation& Preparation, bool Enabled);
    void Observe(const std::vector<powder::Grain>& Positions, double Time);
    void InvalidateCapture();
    FPlantBatchRecoverySnapshot Recovery;
    void RecoveryRevoked(){Recovery.Ticket=0;}
    bool AdmitRecovery(const std::vector<powder::Grain>& P,double Time,int64 Capture,int64 Revision,FGuid Request);
    bool RecoveryValid() const {return Recovery.Ticket>0&&Recovery.Valid&&Recovery.ForeignIds.IsEmpty();}
    void ObserveMix(double Time,bool Automatic,double AppliedRPM);

    bool BeginAdditive(const std::vector<powder::Grain>& P,double Time,int64 CaptureRevision,int64 Revision,bool Scheduled,double Delay);
    void ObserveAdditiveReady(double Time,bool ClosedAndVented);
    bool AdditiveActive() const {return Sequential.AdditiveActive;}
    bool AdditiveScheduled() const {return Sequential.AdditiveActive&&Sequential.AdditiveScheduled;}
    const FPlantMixerFeedSnapshot& MixerStartSnapshot() const {return AdditiveScheduled()?InitialFeed:Feed;}
    const FPlantMixerFeedSnapshot& InitialMixerSnapshot() const {return InitialFeed;}
    int32 ActiveSupplier() const {return Sequential.ActiveSupplier;}
    const FPlantSequentialBatchSnapshot& SequentialSnapshot() const {return Sequential;}
    bool Transition(const std::vector<powder::Grain>& P,double Time,int64 CaptureRevision,int64 Revision,bool Combine);
    void RejectTransition(int64 Revision,const FString& Reason){Sequential.RejectedRevision=Revision;Sequential.Reason=Reason;}

    bool Capture(const std::vector<powder::Grain>& Positions, double Time, int64 Revision);
    bool VerifyOpening(const std::vector<powder::Grain>& Positions, double Time, int64 Revision);
    bool FinalizeDose(const std::vector<powder::Grain>& P,double Time,int64 CaptureRevision,int64 Revision,double TargetKg,double ToleranceKg);
    void RejectDose(int64 Revision,const FString& Reason);
    void SupplyCloseRequested(const std::vector<powder::Grain>& P,double Time,int64 Revision);
    void SupplyClosed(const std::vector<powder::Grain>& P,double Time,int64 Revision);
    const FPlantABatchSnapshot& Snapshot() const { return State; }
    const FPlantMixerFeedSnapshot& MixerSnapshot() const { return Feed; }
    bool BeginMixerFeed(const std::vector<powder::Grain>& P,double Time,int64 CaptureRevision,int64 OpeningRevision);
    bool AdmitMixerStart(const std::vector<powder::Grain>& P,double Time,int64 CaptureRevision,int64 Revision);
    void ReceiverClosed(double Time,int64 Revision);
    void MixerApplied(double Time,int64 Revision,double RPM);
    void RejectDownstream(int64 Revision,const FString& Reason);
private:
    FPlantABatchSnapshot State;
    FPlantSequentialBatchSnapshot Sequential;
    TArray<int32> Labels;
    TArray<uint8> CohortA,CohortB,CohortC;
    double GateHeights[3]{};

    FPlantMixerGeometry Geometry;
    FPlantMixerFeedSnapshot Feed,InitialFeed;
    FPlantTransferSettings Criteria;
    TArray<uint8> ExistingMixer, UnexpectedSeen;
    double StableSince=-1;
    TArray<uint8> IsA, SelectedDose;
    TArray<int32> WeighingIds;
    double WeighingStableSince=-1;

    const FPlantTransferTelemetry& Mass;
    const FPlantFeedPreparation& Preparation;
    double GateZ=0;
    TArray<uint8> ForeignSeen;
    double ReadyMixSeconds=-1;
    double MixLastTime=-1;
    bool MixWasAutomatic=false;
    int64 LastRecoveryAttempt=0;
    void Foreign(int32 Id,const FString& Region);

};
