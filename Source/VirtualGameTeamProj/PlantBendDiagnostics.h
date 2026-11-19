#pragma once
#include "PlantFeedPreparation.h"
#include "powder/plant_pressure.hpp"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include <sstream>
#include <iomanip>

/** Opt-in bounded worker diagnostics. No state writes, control requests or extra physics steps.
 * Trace only the last <=32 particles in B's real downstream route, at most 64 samples.
 * Data stays in memory until purge ends/worker pauses/destructs; no per-step filesystem IO.
 */
class FPlantBendDiagnostics
{
 bool Enabled=false,Flushed=false;FString Directory;std::ostringstream Rows;TArray<int32> Previous;
 double Next=0,MaxCapture=0,TotalCapture=0;unsigned Samples=0;
public:
 FPlantBendDiagnostics(){
#if WITH_DEV_AUTOMATION_TESTS
  Enabled=FParse::Param(FCommandLine::Get(),TEXT("PlantBendDiagnostics"));
  if(Enabled){
   FString Base;FParse::Value(FCommandLine::Get(),TEXT("PlantBendDiagnosticsDir="),Base);
   const FString Saved=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
   Base=Base.IsEmpty()?Saved/TEXT("PlantBendDiagnostics"):FPaths::ConvertRelativePathToFull(Base);
   if(!Base.StartsWith(Saved)){Enabled=false;return;}
   Directory=Base/FString::Printf(TEXT("capture_%llu"),(unsigned long long)FPlatformTime::Cycles64());
   Enabled=IFileManager::Get().MakeDirectory(*Directory,true);
   Rows<<std::setprecision(17)<<"sample,time_s,id,residual,control_revision,x,y,z,vx,vy,vz,air_x,air_y,air_z,nx,ny,nz,wall_dx,wall_dy,wall_dz,route_cell,progress_m,radial_m,flow_m3s,supply_pa,weighing_pa,receiver_pa,probe_readback_wall_ms\n";
  }
#endif
 }
 ~FPlantBendDiagnostics(){Flush();}
 void Flush(){
  if(!Enabled||Flushed||!Samples)return;Flushed=true;
  const double Begin=FPlatformTime::Seconds();
  const bool OK=FFileHelper::SaveStringToFile(UTF8_TO_TCHAR(Rows.str().c_str()),*(Directory/TEXT("particles.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
  const FString Meta=FString::Printf(TEXT("{\"samples\":%u,\"max_capture_wall_ms\":%.6f,\"total_capture_wall_ms\":%.6f,\"flush_wall_ms\":%.6f,\"saved\":%s}"),Samples,MaxCapture*1000,TotalCapture*1000,(FPlatformTime::Seconds()-Begin)*1000,OK?TEXT("true"):TEXT("false"));
  FFileHelper::SaveStringToFile(Meta,*(Directory/TEXT("timing.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
  UE_LOG(LogTemp,Display,TEXT("BEND_DIAGNOSTICS samples=%u max_ms=%.3f directory=%s"),Samples,MaxCapture*1000,*Directory);
 }
 void Capture(powder::GranularGPU& GPU,const std::vector<powder::Grain>& P,const std::vector<powder::Grain>& V,
  const FPlantFeedPreparation& Prep,const powder::GranularConfig& C,const std::vector<powder::PlantRoute>& Routes,
  const powder::PlantPressureState& Pressure,int64 Revision,int32 Residual){
  if(!Enabled||Flushed||Samples>=64||!Pressure.purge.enabled||Pressure.purge.supplier!=1||Residual>32||Residual<0||GPU.time()<Next)return;
  const double Begin=FPlatformTime::Seconds();Next=GPU.time()+.02;
  try{
   if(P.size()!=V.size())throw std::runtime_error("bend diagnostic snapshot mismatch");
   TArray<int32> IDs;for(int32 I=0;I<int32(P.size());++I)if(Prep.SupplyPath(P[I])==1)IDs.Add(I);
   if(IDs.Num()!=Residual||IDs.Num()>32)throw std::runtime_error("bend diagnostic region mismatch");
   if(IDs.IsEmpty())IDs=Previous;if(IDs.IsEmpty())return;Previous=IDs;
   std::vector<powder::Grain> Points;for(int32 I:IDs)Points.push_back(P[I]);
   const auto Air=GPU.plant_air_snapshot();const auto Walls=GPU.inspect_boundaries(Points,GPU.angle(),false,true);
   const double Duration=FPlatformTime::Seconds()-Begin;
   const auto Path=powder::plant_supply_purge_path(C.plant_data,Routes,1);const auto& R=Routes[Path.route];
   for(int32 J=0;J<IDs.Num();++J){const int32 I=IDs[J];const auto Q=P[I],Vel=V[I],A=Air[I];const auto W=Walls[J];double Best=DBL_MAX,Progress=0,Along=0;int Cell=-1;
    for(size_t K=0;K<R.cells.size();++K){auto U=C.plant_data[R.cells[K]],T=C.plant_data[R.cells[K]+1];const FVector3d Axis(T.x-U.x,T.y-U.y,T.z-U.z),D(Q.x-U.x,Q.y-U.y,Q.z-U.z);const double Length=Axis.Length(),F=FMath::Clamp(FVector3d::DotProduct(D,Axis)/(Length*Length),0.,1.),Dist=(D-F*Axis).Length();if(Dist<Best){Best=Dist;Progress=Along+F*Length;Cell=int(K);}Along+=Length;}
    Rows<<Samples<<','<<GPU.time()<<','<<I<<','<<Residual<<','<<Revision<<','<<Q.x<<','<<Q.y<<','<<Q.z<<','<<Vel.x<<','<<Vel.y<<','<<Vel.z<<','<<A.x<<','<<A.y<<','<<A.z<<','<<W.normal.x<<','<<W.normal.y<<','<<W.normal.z<<','<<W.position.x-Q.x<<','<<W.position.y-Q.y<<','<<W.position.z-Q.z<<','<<Cell<<','<<Progress<<','<<Best<<','<<Pressure.purge_m3s<<','<<Pressure.applied.supply<<','<<Pressure.applied.weighing<<','<<Pressure.applied.receiver<<','<<Duration*1000<<'\n';
   }
   const double FullDuration=FPlatformTime::Seconds()-Begin;MaxCapture=FMath::Max(MaxCapture,FullDuration);TotalCapture+=FullDuration;
   ++Samples;if(Residual==0||Samples>=64)Flush();
  }catch(const std::exception& E){Flush();Enabled=false;UE_LOG(LogTemp,Warning,TEXT("BEND_DIAGNOSTICS disabled: %hs"),E.what());}
 }
};
