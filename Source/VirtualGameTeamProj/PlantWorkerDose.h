#pragma once
#include "ProcessControlTypes.h"

/** Worker-thread-only controller. It only removes a supply request, never opens anything. */
class FPlantWorkerDose
{
public:
 explicit FPlantWorkerDose(bool Supported):Supported(Supported){State.Supported=Supported;}
 FPlantDoseCutoffSnapshot State;
 void Sync(const FPlantControlTargets& C,int32 Active,int64 Capture,double Sim,double Now,double ParticleKg,double AvailableKg);
 uint32 Requests(const FPlantControlTargets& C) const;
 void Opened(int32 Supplier,double Sim);
 bool Observe(double Passed,double Spill,int32 Source,double Sim,double Now,int64 NextPublicationSequence);
 void Closed(int32 Supplier,double Sim);
 void ObservationUnavailable(){Cancel(TEXT("작업자 제동 피더 위치·속도 관찰 미확인"));}
private:
 bool Supported=false;
 int64 HighWater=0;
 double PreviousPassed=0,PreviousTime=-1;
 void Cancel(const TCHAR* Why);
};
