#pragma once
#include "PlantSimulationSnapshot.h"
#include "powder/plant.hpp"

/** Read-only reduction of the uploaded carrier field. Never solves a new flow or changes model data. */
class FPlantSupplyPressureTelemetry
{
 struct FPath {int32 Route=-1,Gate=-1,Cell=-1;FVector Before,After;FString Error;};
 TArray<FPath> Paths;
public:
 FPlantSupplyPressureTelemetry(const std::vector<powder::Grain>& Data,const std::vector<powder::PlantRoute>& Routes);
 TArray<FPlantSupplyValvePressure> Observe(const std::vector<powder::Grain>& Data,const std::vector<powder::PlantRoute>& Routes,
  const FVector& AppliedPa,uint32 Open,uint32 Air,bool Regulated,int32 PurgeSupplier,bool Purging,const TArray<double>& Sources={}) const;
};
