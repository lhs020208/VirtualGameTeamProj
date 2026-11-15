#pragma once
#include "CoreMinimal.h"
#include "PlantSimulationSnapshot.h"
namespace PlantDoseTrace {
 bool Enabled();
 void Write(const TCHAR* Event,int32 Supplier,double Sim,int64 Sequence,int64 Revision,int64 Batch,double Passed,double Spill,double Estimate=-1,double PublishedWall=0,const FString& Request=FString());
 void Snapshot(const TCHAR* Event,const FPlantSimulationSnapshot& S,double Estimate=-1);
}
