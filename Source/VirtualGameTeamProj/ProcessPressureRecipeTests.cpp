#include "ProcessRecipe.h"
#include "ProcessControlService.h"
#include "PlantSupplyPressureTelemetry.h"
#include "powder/plant_pressure.hpp"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPressureRecipeVersion,"ProcessUI.PressureRecipe.VersionAndAtomicTargets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPressureRecipeVersion::RunTest(const FString&)
{
 FProcessRecipe R;R.Name=TEXT("압력 복원 시험");R.Mode=EProcessBatchMode::TargetMass;R.TargetAKg=.04;R.ToleranceAKg=.002;R.TargetRPM=7;R.MixingSeconds=1;R.TransferPressurePa.Z=-500.123456789;
 FProcessRecipe Out;FString Why;TestTrue(TEXT("v2 exact round trip"),ProcessRecipe::Decode(ProcessRecipe::Encode(R),Out,Why));TestTrue(TEXT("all values identical"),ProcessRecipe::Same(R,Out));
 TSharedPtr<FJsonObject> O;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ProcessRecipe::Encode(R)),O);
 auto JSON=[&](){FString S;FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S));return S;};
 O->SetNumberField(TEXT("version"),1);O->RemoveField(TEXT("hopper_pressure_targets"));O->GetObjectField(TEXT("units"))->RemoveField(TEXT("pressure"));
 TestTrue(TEXT("existing v1 accepted"),ProcessRecipe::Decode(JSON(),Out,Why));TestEqual(TEXT("v1 original feed"),Out.FeedPressurePa,FVector(500,-500,0));TestEqual(TEXT("v1 original transfer"),Out.TransferPressurePa,FVector(0,500,-1000));
 const auto Original=ProcessRecipe::Encode(Out);
 O->SetNumberField(TEXT("version"),2);TestFalse(TEXT("v2 missing pressure is malformed"),ProcessRecipe::Decode(JSON(),Out,Why));TestEqual(TEXT("bad load atomic"),ProcessRecipe::Encode(Out),Original);
 FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ProcessRecipe::Encode(R)),O);O->GetObjectField(TEXT("units"))->SetStringField(TEXT("pressure"),TEXT("kPa_g"));TestFalse(TEXT("wrong stored unit rejected"),ProcessRecipe::Decode(JSON(),Out,Why));
 auto* S=NewObject<UProcessControlService>();S->State.Snapshot.RequestedControlRevision=42;
 TestTrue(TEXT("apply only config"),S->ApplyRecipe(R,Why));TestEqual(TEXT("exact pressure applied"),S->GetAppliedRecipe().TransferPressurePa.Z,R.TransferPressurePa.Z);TestEqual(TEXT("no request"),S->State.History.Num(),0);TestEqual(TEXT("no worker change"),S->State.Snapshot.RequestedControlRevision,int64(42));
 const auto Good=ProcessRecipe::Encode(S->GetAppliedRecipe());for(double Bad:{.001,-1000.1,std::numeric_limits<double>::quiet_NaN()}){R.TransferPressurePa.Z=Bad;TestFalse(TEXT("out of range/NaN refused"),S->ApplyRecipe(R,Why));TestEqual(TEXT("no partial apply"),ProcessRecipe::Encode(S->GetAppliedRecipe()),Good);}
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSupplyValveEstimate,"ProcessUI.PressureRecipe.UploadedFieldAndGateTaps",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSupplyValveEstimate::RunTest(const FString&)
{try{
 const FString Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("SourceAssets/ProcessPlant_20261111_LowPoly/Simulation"));
 auto D=powder::read_plant_data(TCHAR_TO_UTF8(*(Root/TEXT("runtime.plant"))));auto R=powder::read_plant_routes(TCHAR_TO_UTF8(*(Root/TEXT("runtime.routes"))));FPlantSupplyPressureTelemetry T(D,R);
 for(int I=0;I<6;++I){powder::plant_set_regulated_pressures(D,R,{500,-500,0},1u<<I,1u<<I,false,false,false);const auto Before=D;
  auto V=T.Observe(D,R,FVector(500,-500,0),1u<<I,1u<<I,true,0,false);TestTrue(TEXT("real gate mapped"),V[I].Mapped);TestTrue(TEXT("existing field estimated"),V[I].Valid);TestEqual(TEXT("all real gates in cell11"),V[I].GateCell,11);
  if(!V[I].Valid){AddError(V[I].Reason);continue;}
  TestTrue(TEXT("pressure before decreases along positive flow"),V[I].BeforeGaugePa<500&&V[I].BeforeGaugePa>V[I].AfterGaugePa);TestTrue(TEXT("tap drop smaller than complete hopper difference"),V[I].DifferencePa>0&&V[I].DifferencePa<1000);
  TestEqual(TEXT("reported difference uses the two estimates"),V[I].DifferencePa,V[I].BeforeGaugePa-V[I].AfterGaugePa);TestTrue(TEXT("uploaded float field closes boundary"),FMath::Abs(V[I].BoundaryResidualPa)<.01);
  const int At=R[V[I].Route].cells[V[I].GateCell];const auto A=D[At],B=D[At+1];const double Expected=powder::plant_cell_loss(D[At+2].x,std::hypot(std::hypot(B.x-A.x,B.y-A.y),B.z-A.z),FMath::Min(A.w,B.w),D[At+2].y);
  TestTrue(TEXT("no invented gate K: existing cell loss only"),FMath::Abs(V[I].DifferencePa-Expected)<1e-8);TestTrue(TEXT("read-only telemetry"),FMemory::Memcmp(D.data(),Before.data(),D.size()*sizeof(powder::Grain))==0);
  auto Closed=T.Observe(D,R,FVector(500,-500,0),0,0,true,I,false);TestFalse(TEXT("closed drops previous estimate"),Closed[I].Valid);TestEqual(TEXT("no stale tap value"),Closed[I].BeforeGaugePa,0.);TestTrue(TEXT("closed reason"),Closed[I].Reason.Contains(TEXT("닫힘")));
  auto Purge=T.Observe(D,R,FVector(0,-500,0),0,0,true,I,true);TestFalse(TEXT("purge is not valve flow"),Purge[I].Valid);TestTrue(TEXT("purge distinctly named"),Purge[I].Reason.Contains(TEXT("하류 연결부 공기")));
 }
 powder::plant_set_regulated_pressures(D,R,{-500,500,0},1,1,false,false,false);auto Reverse=T.Observe(D,R,FVector(-500,500,0),1,1,true,0,false);TestTrue(TEXT("signed reverse flow retained"),Reverse[0].Valid&&Reverse[0].DifferencePa<0);
 auto BadRoutes=R;BadRoutes[0].cells.erase(BadRoutes[0].cells.begin()+11);FPlantSupplyPressureTelemetry Bad(D,BadRoutes);TestFalse(TEXT("unmapped route never invents tap"),Bad.Observe(D,BadRoutes,FVector(-500,500,0),1,1,true,0,false)[0].Valid);
 }catch(const std::exception& E){AddError(UTF8_TO_TCHAR(E.what()));return false;}return true;
}
#endif
