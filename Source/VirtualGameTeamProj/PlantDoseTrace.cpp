#include "PlantDoseTrace.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
namespace { TAutoConsoleVariable<int32> DoseTrace(TEXT("plant.DoseTrace"),0,TEXT("Bounded B dosing event trace; no particles or per-step file I/O."),ECVF_Default); }
bool PlantDoseTrace::Enabled(){return DoseTrace.GetValueOnAnyThread()!=0;}
void PlantDoseTrace::Write(const TCHAR* Event,int32 Supplier,double Sim,int64 Sequence,int64 Revision,int64 Batch,double Passed,double Spill,double Estimate,double PublishedWall,const FString& Request){
 if(Supplier!=1||!Enabled())return;
 UE_LOG(LogTemp,Display,TEXT("DOSE_TRACE event=%s supplier=%d wall=%.9f sim=%.9f seq=%lld revision=%lld batch=%lld passed_kg=%.12g spill_kg=%.12g estimate_kg=%.12g published_wall=%.9f request=%s"),Event,Supplier,FPlatformTime::Seconds(),Sim,Sequence,Revision,Batch,Passed,Spill,Estimate,PublishedWall,*Request);
}
void PlantDoseTrace::Snapshot(const TCHAR* Event,const FPlantSimulationSnapshot& S,double Estimate){
 const auto& B=S.SupplyABatch;Write(Event,S.SequentialBatch.ActiveSupplier,S.PhysicalSeconds,S.Sequence,S.ProcessedControlRevision,B.CaptureRevision,(B.InitialCount-B.InventorySourceCount)*S.ModelParticleMassKg,S.SupplyBBrake.PredictedSpillKg,Estimate,S.PublishedWallSeconds);
}
