#pragma once

#include "PlantSimulationSnapshot.h"
#include "powder/plant.hpp"

enum class EPlantMassRegion : uint8 { Outside, Weighing, Pipe, Receiver, Invalid };

/** Worker-owned observer. No writes to particles, geometry, flow, valves or the GPU. */
class FPlantTransferTelemetry
{
public:
    FPlantTransferTelemetry(const powder::GranularConfig& Config, const std::vector<powder::PlantRoute>& Routes,
        const FPlantTransferSettings& Settings);
    EPlantMassRegion Classify(const powder::Grain& Position) const;
    void ResetConnection(int64 Connection);
    // Must be called on a synchronous GPU snapshot BEFORE applying closed -> open.
    void Begin(const std::vector<powder::Grain>& Positions, double Time, int64 Revision, bool ReceiverOpen);
    void Observe(const std::vector<powder::Grain>& Positions, double Time, bool OutletOpen, bool ReceiverOpen);
    void ReceiverDischarging();
    void Publish(FPlantSimulationSnapshot& Snapshot) const;
    double ParticleMassKg = 0;
private:
    struct FTube { FVector3d A, B; double RA, RB; };
    TArray<FTube> Pipes;
    FBox3d PipeBounds{ForceInit};
    double WeighingGateZ = .6, ReceiverGateZ = 2.16;
    bool LowPolyReceiver = false;
    powder::Grain ReceiverInletEnd{2.77f,0,2.682f,.061f};
    size_t ParticleCount = 0;
    FString GeometryError;
    TArray<EPlantMassRegion> Regions;
    TArray<uint8> Membership; // 1 = this transfer; 2 = pre-existing receiver inventory.
    FPlantMassValue Weighing, Pipe, Receiver;
    FPlantTransferSnapshot Transfer;
    FPlantTransferSettings Criteria;
    int64 NextId = 0;
    double StableSince = -1, LastTime = -1;
    bool Collect(const std::vector<powder::Grain>& Positions);
    void Indeterminate(const FString& Reason);
};
