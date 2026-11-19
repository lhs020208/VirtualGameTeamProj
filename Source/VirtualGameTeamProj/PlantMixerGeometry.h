#pragma once
#include "CoreMinimal.h"
#include "powder/plant.hpp"

enum class EPlantMixerRegion : uint8 { Outside, Connector, Mixer, Invalid };

// Observation only. Coordinates and wall probes are from the active LowPoly runtime BVH.
// The inlet neck above the chamber lid belongs to the connector, not arrived mixer mass.
class FPlantMixerGeometry
{
public:
 explicit FPlantMixerGeometry(const powder::GranularConfig& Config, bool Enabled);
 EPlantMixerRegion Classify(const powder::Grain& Position) const;
 FString Error;
 bool Supported=false;
 double ReceiverGateZ=0, ChamberTopZ=double(1.2675f);
private:
 TArray<FVector2d> ConnectorProfile; // z, inner bore radius
};
