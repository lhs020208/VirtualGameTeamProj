#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "powder/plant_pressure.hpp"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBendContactNormal,"ProcessUI.Bend.GPUContactNormalAndProgress",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBendContactNormal::RunTest(const FString&){try{
 const FString Root=FPaths::ProjectDir()/TEXT("SourceAssets/ProcessPlant_20261111_LowPoly/Simulation");
 powder::GranularConfig C;C.plant=true;C.plant_threaded=true;C.plant_wall_mode=5;C.kernel_optimize=true;C.count=1;C.radius=.002f;C.dt=.00001f;C.iterations=8;
 C.plant_data=powder::read_plant_data(TCHAR_TO_UTF8(*(Root/TEXT("runtime.plant"))));const auto Routes=powder::read_plant_routes(TCHAR_TO_UTF8(*(Root/TEXT("runtime.routes"))));
 const auto Pressure=powder::plant_set_regulated_pressures(C.plant_data,Routes,{0,-500,0},0,0,false,false,false,{true,500,1});
 const powder::Grain P{.308278083801f,1.40349125862f,2.02368569374f,0};
 powder::GranularGPU GPU(C,{P},{{0,0,0,0}});const auto H=GPU.inspect_boundaries({P},0,false,true)[0];
 const FVector3d N(H.normal.x,H.normal.y,H.normal.z),Reference(-.18356908644,-.52460604789,.83131876258);
 TestTrue(TEXT("near-shell friction normal agrees with actual triangle, not quantized displacement axis"),FVector3d::DotProduct(N,Reference)>.999);
 const auto Air=GPU.plant_air_snapshot()[0];const FVector3d Flow(Air.x,Air.y,Air.z);
 TestTrue(TEXT("wall tangent carrier flow is not opposed by a false normal"),FMath::Abs(FVector3d::DotProduct(N,Flow.GetSafeNormal()))<.001);
 const auto Repeat=GPU.inspect_boundaries({P},0,false,true)[0];TestEqual(TEXT("cached normal repeat"),Repeat.normal.x,H.normal.x);
 const auto Untouched=GPU.snapshot()[0];TestEqual(TEXT("diagnostics leave particle position unchanged"),Untouched.x,P.x);TestEqual(TEXT("diagnostics leave velocity unchanged"),GPU.velocity_snapshot()[0].x,0.f);
 TestTrue(TEXT("physical projection remains submicrometre, no invented position displacement"),FVector3d(H.position.x-P.x,H.position.y-P.y,H.position.z-P.z).Length()<1e-6);
 GPU.advance(1000,0,false);GPU.advance(1000,0,false);const auto After=GPU.snapshot()[0],V=GPU.velocity_snapshot()[0];
 const auto A=C.plant_data[Routes[1].cells[40]],B=C.plant_data[Routes[1].cells[40]+1];const FVector3d Axis=FVector3d(B.x-A.x,B.y-A.y,B.z-A.z).GetSafeNormal();
 TestTrue(TEXT("previously stalled grain travels downstream without increased pressure"),FVector3d::DotProduct(FVector3d(After.x-P.x,After.y-P.y,After.z-P.z),Axis)>.015);
 TestTrue(TEXT("downstream motion is sustained, not wall jitter"),FVector3d::DotProduct(FVector3d(V.x,V.y,V.z),Axis)>1.);
 TestEqual(TEXT("particle ID/count preserved"),GPU.snapshot().size(),size_t(1));TestTrue(TEXT("same purge flow"),FMath::Abs(Pressure.purge_m3s-.06585775809701218)<1e-12);
 }catch(const std::exception& E){AddError(UTF8_TO_TCHAR(E.what()));return false;}return true;}
#endif
