#pragma once

#include "PlantTransferTelemetry.h"
#include "ProcessControlTypes.h"

/** Worker-owned admission check. Observes real particles; never moves/removes them or changes physics. */
class FPlantFeedPreparation
{
public:
    FPlantFeedPreparation(const powder::GranularConfig& Config, const std::vector<powder::PlantRoute>& Routes,
        const FPlantTransferTelemetry& Telemetry, double RequiredStableSeconds, bool Enabled);
    // A supply residual is downstream of a real supply gate and upstream of WH ownership.
    int32 SupplyPath(const powder::Grain& Position) const;
    int32 SupplyAInventory(const std::vector<powder::Grain>& Positions) const;
    void SyncIntent(const FPlantControlTargets& Control, double Time, double WallTime);
    bool Evaluate(const std::vector<powder::Grain>& Positions, double Time, double WallTime,
        const FPlantControlTargets& Control, uint32 AppliedSuppliers, bool ContinuingTransfer, bool PurgeApplied=false, double PurgeFlow=0, double PurgePressure=0);
    void Opened(double BaselineTime, double AppliedTime);
    const FPlantFeedPreparationSnapshot& Snapshot() const { return State; }
private:
    struct FCell { FVector3d A, B; double RA, RB; bool Rectangular; };
    struct FSupply { TArray<FCell> Cells; FBox3d Bounds{ForceInit}; powder::Grain Gate{}; };
    TArray<FSupply> Supplies;
    const FPlantTransferTelemetry& Mass;
    size_t Count;
    double Radius, RequiredStable, StableSince = -1, LastTime = -1;
    int64 LastControlRevision = -1;
    FString GeometryError;
    TArray<int32> StableIds;
    FPlantFeedPreparationSnapshot State;
    void ResetStability();
    bool Fail(const FString& Reason, EPlantFeedPreparationState Result = EPlantFeedPreparationState::Failed);
};
