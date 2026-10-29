#include "PowderCoupledTest.h"
#include "powder/coupled.hpp"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Engine/Engine.h"
#include "UnrealClient.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include <algorithm>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>
namespace {struct FCoupledPoint {powder::Vec3 Position;double Radius;unsigned Type;std::uint64_t Id;};}
struct FCoupledRuntime {
 std::thread Thread;std::mutex Mutex;std::atomic<bool> Quit{false},Run{true},Pulse{false};std::atomic<double> Drop{25},Rate{.1};
 std::vector<FCoupledPoint> Points;powder::CoupledReport Report;std::string Error;double Wall=0,Exited=0,AppliedTemperature=0,AppliedDt=0,AppliedDrop=0,Supplied=0,Filter=0,MinimumDt=0,Backflow=0,Outlet=0,ContactKn=0,ContactCn=0,WallGamma=0,MaxFilter=0,MinimumLateFilter=-1;int Mode=0;std::string Stage,SpeciesJson="{}";uint64 Generation=0;
 FCoupledRuntime(powder::CoupledWorld World,double Dt,int Axis,bool Start,double RequestedRate,double Pressure,int Experiment){Mode=Experiment;ContactKn=World.dem.wall_models.at(0).linear.kn;ContactCn=World.dem.wall_models.at(0).linear.cn;WallGamma=World.dem.wall_models.at(0).adhesion_gamma_j_m2;Run=Start;Rate=RequestedRate;Drop=Pressure;AppliedTemperature=World.gas.config.temperature_k;AppliedDt=Dt;AppliedDrop=Pressure;
 Thread=std::thread([this,W=std::move(World),Dt,Axis]()mutable{
  auto Publish=[&]{std::lock_guard<std::mutex> Lock(Mutex);Report=W.report();MinimumDt=W.minimum_step_s;Exited=W.exited_mass;Backflow=W.exited_by_face[2*Axis];Outlet=W.exited_by_face[2*Axis+1];Supplied=W.injected_mass;Filter=(Mode==2||Mode==5)?powder::coupled_filter_mass(W):0;MaxFilter=std::max(MaxFilter,Filter);if(Report.time>=.2&&(Mode==2||Mode==5))MinimumLateFilter=MinimumLateFilter<0?Filter:std::min(MinimumLateFilter,Filter);Stage=Mode?powder::coupled_experiment_stage(Mode,Report.time):"manual";AppliedDrop=101325-W.gas.config.boundary_pa[2*Axis+1];std::ostringstream Species;Species<<std::setprecision(17)<<"[";bool First=true;for(const auto& S:W.injected_by_type){if(!First)Species<<",";First=false;double Left=W.exited_by_type.count(S.first)?W.exited_by_type.at(S.first):0;Species<<"{\"type\":"<<S.first<<",\"supplied_kg\":"<<S.second<<",\"exited_kg\":"<<Left<<",\"retained_kg\":"<<S.second-Left<<"}";}Species<<"]";SpeciesJson=Species.str();Points.clear();for(const auto& B:W.dem.bodies)for(const auto& P:powder::clump_particles(B))Points.push_back({P.position_m,P.diameter_m/2,B.type,B.id});++Generation;};
  try{auto InjectionTemplate=W.dem.bodies.front();Publish();std::uint64_t NextId=100000;double Active=0;
   while(!Quit){if(!Run){std::this_thread::sleep_for(std::chrono::milliseconds(2));continue;}auto Begin=std::chrono::steady_clock::now();
    double D=Drop.load();if(!std::isfinite(D)||std::abs(D)>90000)throw std::invalid_argument("pressure drop must be finite within +/-90000 Pa");if(Mode==0)W.gas.config.boundary_pa[2*Axis+1]=101325-D;
    if(Pulse.exchange(false)&&Mode==0){std::vector<powder::ClumpBody> Added;for(int I=0;I<8;++I){auto B=InjectionTemplate;B.id=NextId++;B.velocity_m_s={};B.angular_momentum_world={};B.position_m={.025+I*.009,.095,.095};Added.push_back(B);}W.inject(Added);}
    for(int I=0;I<16&&!Quit;++I){if(Mode)powder::step_coupled_experiment(W,Mode,Dt);else W.step(Dt);}
    double Compute=std::chrono::duration<double>(std::chrono::steady_clock::now()-Begin).count();double Target=Rate.load();if(!std::isfinite(Target)||Target<=0||Target>1)throw std::invalid_argument("target rate must be (0,1]");
    double Sleep=16*Dt/Target-Compute;if(Sleep>0)std::this_thread::sleep_for(std::chrono::duration<double>(Sleep));Active+=std::chrono::duration<double>(std::chrono::steady_clock::now()-Begin).count();
    {std::lock_guard<std::mutex> Lock(Mutex);Wall=Active;}Publish();
   }
  }catch(const std::exception& E){std::lock_guard<std::mutex> Lock(Mutex);Error=E.what();Run=false;}
 });}
 ~FCoupledRuntime(){Quit=true;if(Thread.joinable())Thread.join();}
};
APowderCoupledTest::APowderCoupledTest(){PrimaryActorTick.bCanEverTick=true;RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));for(int I=0;I<3;++I){auto* G=CreateDefaultSubobject<UInstancedStaticMeshComponent>(*FString::Printf(TEXT("Grains%d"),I));G->SetupAttachment(RootComponent);G->SetCollisionEnabled(ECollisionEnabled::NoCollision);G->SetCastShadow(false);Groups.Add(G);}Walls=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ReferenceWalls"));Walls->SetupAttachment(RootComponent);Walls->SetCollisionEnabled(ECollisionEnabled::NoCollision);Walls->SetCastShadow(false);}
APowderCoupledTest::~APowderCoupledTest(){Stop();}
void APowderCoupledTest::Stop(){delete Runtime;Runtime=nullptr;}
void APowderCoupledTest::ResetSimulation(){Stop();Error.Empty();PhysicalSeconds=ActualRate=GasMassErrorKg=ParticleMassErrorKg=ExitedMassKg=SuppliedMassKg=FilterMassKg=AppliedPressureDropPa=0;ExperimentStage.Empty();Generation=0;
 try{if(!std::isfinite(FixedDt)||FixedDt<=0||FixedDt>1e-4||ParticleCount<1||ParticleCount>512||FlowAxis<0||FlowAxis>2||!std::isfinite(DiameterScale)||DiameterScale<.5||DiameterScale>1.2)throw std::invalid_argument("invalid coupled setup");
  if(Experiment<0||Experiment>6)throw std::invalid_argument("experiment must be 0..6");
  auto W=Experiment?powder::coupled_experiment(Experiment):powder::coupled_demo(unsigned(ParticleCount),PressureDropPa,PairAdhesionGamma,GasTemperatureK,PartialObstruction,FlowAxis);
  const int AppliedAxis=Experiment?((Experiment==2||Experiment==5)?2:0):FlowAxis;
  const double StepDt=Experiment?1e-7:FixedDt;
  DisplayUnitsPerMetre=Experiment?10000:1000;auto Extent=W.gas.extent();DomainExtentM=FVector(Extent.x,Extent.y,Extent.z);
  if(!Experiment){
  if(ExtraPressureFace < -1 || ExtraPressureFace>5 || (ExtraPressureFace>=0&&ExtraPressureFace/2==FlowAxis))throw std::invalid_argument("extra pressure face must differ from flow axis");
  if(ExtraPressureFace>=0){W.gas.config.boundary_pa[ExtraPressureFace]=ExtraPressurePa;W.gas.config.validate();W.dem.planes.erase(std::remove_if(W.dem.planes.begin(),W.dem.planes.end(),[&](const auto& P){return P.id==uint64(ExtraPressureFace+1);}),W.dem.planes.end());}
  for(auto& Entry:W.dem.wall_models)Entry.second.adhesion_gamma_j_m2=WallAdhesionGamma;
  double Density[3]{DensitiesKgM3.X,DensitiesKgM3.Y,DensitiesKgM3.Z};for(double D:Density)if(!std::isfinite(D)||D<100||D>10000)throw std::invalid_argument("density must be 100..10000 kg/m3");
  for(auto& B:W.dem.bodies){B.density_kg_m3=Density[B.type];for(auto& P:B.pebbles){P.diameter_m*=DiameterScale;P.local_m.x*=DiameterScale;P.local_m.y*=DiameterScale;P.local_m.z*=DiameterScale;}}
  }
  W.initialize();auto* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/DEM/SM_GrainSphere.SM_GrainSphere"));if(!Mesh)Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
  auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/M_BulkFlow.M_BulkFlow"));FLinearColor Colors[3]{FLinearColor(.8f,.05f,.02f),FLinearColor(.02f,.25f,.9f),FLinearColor(.9f,.7f,.02f)};
  for(int I=0;I<3;++I){Groups[I]->ClearInstances();Groups[I]->SetStaticMesh(Mesh);if(Material){Groups[I]->SetMaterial(0,Material);if(auto* M=Groups[I]->CreateDynamicMaterialInstance(0))M->SetVectorParameterValue(TEXT("Tint"),Colors[I]);}}
  Walls->ClearInstances();Walls->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));if(Material){Walls->SetMaterial(0,Material);if(auto* M=Walls->CreateDynamicMaterialInstance(0))M->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.15f,.2f,.22f));}
  // Display magnified 10x (manual) or 100x (small preset); solver remains SI.
  auto Add=[&](FVector Centre,FVector Size){Walls->AddInstance(FTransform(FQuat::Identity,Centre*DisplayUnitsPerMetre,Size*(DisplayUnitsPerMetre/100)));};
  const FVector E=DomainExtentM;const double VisibleHeight=(Experiment==2||Experiment==5)?.012:E.Z;const double Thickness=Experiment?.0001:.004;
  if((AppliedAxis!=2||Experiment==2||Experiment==5)&&(Experiment||ExtraPressureFace!=4))Add(FVector(E.X/2,E.Y/2,-Thickness/2),FVector(E.X,E.Y,Thickness));
  if(AppliedAxis!=1&&(Experiment||ExtraPressureFace!=3))Add(FVector(E.X/2,E.Y+Thickness/2,VisibleHeight/2),FVector(E.X,Thickness,VisibleHeight));
  if(Experiment==2||Experiment==5){
   // Filter collision is a full ideal plane; display its rim to keep captured grains visible.
   const double Rim=.0003;for(double X:{Rim/2,E.X-Rim/2})Add(FVector(X,E.Y/2,.01),FVector(Rim,E.Y,Thickness));for(double Y:{Rim/2,E.Y-Rim/2})Add(FVector(E.X/2,Y,.01),FVector(E.X,Rim,Thickness));
  }
  for(const auto& B:W.dem.boxes)Add(FVector(B.centre_m.x,B.centre_m.y,B.centre_m.z),FVector(B.half_m.x*2,B.half_m.y*2,B.half_m.z*2));
  for(const auto& B:W.dem.bodies)for(const auto& P:powder::clump_particles(B)){auto* G=Groups[B.type%3].Get();double MeshDiameter=G->GetStaticMesh()?G->GetStaticMesh()->GetBounds().BoxExtent.GetMax()*2:100.;G->AddInstance(FTransform(FQuat::Identity,FVector(P.position_m.x,P.position_m.y,P.position_m.z)*DisplayUnitsPerMetre,FVector(P.diameter_m*DisplayUnitsPerMetre/MeshDiameter)));}
  UpdateCamera();if(HasActorBegunPlay())Runtime=new FCoupledRuntime(std::move(W),StepDt,AppliedAxis,Running,TargetRate,PressureDropPa,Experiment);
 }catch(const std::exception& E){Error=UTF8_TO_TCHAR(E.what());Running=false;}
}
void APowderCoupledTest::UpdateCamera(){if(!ViewCamera)return;FVector E=DomainExtentM;if(Experiment==2||Experiment==5)E.Z=.012;ViewCamera->SetActorLocation(GetActorTransform().TransformPosition(E*DisplayUnitsPerMetre*FVector(.95,-1.8,1.6)));ViewCamera->SetActorRotation((GetActorTransform().TransformPosition(E*DisplayUnitsPerMetre*FVector(.5,.5,.3))-ViewCamera->GetActorLocation()).Rotation());}
void APowderCoupledTest::BeginPlay(){Super::BeginPlay();FParse::Value(FCommandLine::Get(),TEXT("CoupledExperiment="),Experiment);FParse::Value(FCommandLine::Get(),TEXT("CoupledSeconds="),StopAt);FParse::Value(FCommandLine::Get(),TEXT("CoupledOutput="),ExportDirectory);ResetSimulation();ViewCamera=GetWorld()->SpawnActor<ACameraActor>();UpdateCamera();if(auto* PC=GetWorld()->GetFirstPlayerController())PC->SetViewTarget(ViewCamera);}

void APowderCoupledTest::Show(){if(!Runtime)return;std::lock_guard<std::mutex> Lock(Runtime->Mutex);if(!Runtime->Error.empty()){Error=UTF8_TO_TCHAR(Runtime->Error.c_str());Running=false;}if(Generation==Runtime->Generation)return;Generation=Runtime->Generation;const auto& R=Runtime->Report;PhysicalSeconds=R.time;ActualRate=Runtime->Wall>0?R.time/Runtime->Wall:0;GasMassErrorKg=R.gas_balance_error;ParticleMassErrorKg=R.particle_balance_error;ExitedMassKg=Runtime->Exited;AppliedPressureDropPa=Runtime->AppliedDrop;SuppliedMassKg=Runtime->Supplied;FilterMassKg=Runtime->Filter;ExperimentStage=UTF8_TO_TCHAR(Runtime->Stage.c_str());
 for(auto G:Groups)G->ClearInstances();for(const auto& P:Runtime->Points){auto* G=Groups[P.Type%3].Get();double MeshDiameter=G->GetStaticMesh()?G->GetStaticMesh()->GetBounds().BoxExtent.GetMax()*2:100.;if(MeshDiameter<=0)MeshDiameter=100.;G->AddInstance(FTransform(FQuat::Identity,FVector(P.Position.x,P.Position.y,P.Position.z)*DisplayUnitsPerMetre,FVector(P.Radius*2*DisplayUnitsPerMetre/MeshDiameter)));}}
void APowderCoupledTest::Tick(float D){Super::Tick(D);if(auto* PC=GetWorld()->GetFirstPlayerController()){if(PC->WasInputKeyJustPressed(EKeys::SpaceBar))Running=!Running;if(PC->WasInputKeyJustPressed(EKeys::R))ResetSimulation();if(PC->WasInputKeyJustPressed(EKeys::I))InjectPulse();if(PC->WasInputKeyJustPressed(EKeys::O))ExportSnapshot();}if(Runtime){Runtime->Run=Running;Runtime->Drop=PressureDropPa;Runtime->Rate=TargetRate;}Show();if(GEngine&&Experiment)GEngine->AddOnScreenDebugMessage(2902,0,FColor::Yellow,FString::Printf(TEXT("Experiment %d: %s | supplied %.6g kg | retained %.6g kg | exited %.6g kg | filter contact %.6g kg"),Experiment,*ExperimentStage,SuppliedMassKg,SuppliedMassKg-ExitedMassKg,ExitedMassKg,FilterMassKg));if(GEngine)GEngine->AddOnScreenDebugMessage(2901,0,Error.IsEmpty()?FColor::Cyan:FColor::Red,FString::Printf(TEXT("3D gas + rigid DEM | CPU reference | t %.5fs | %.4fx | dP %.1f Pa | gas err %.2g kg | particle err %.2g kg\nVirtual materials, no real-material calibration | Space pause / R reset / I inject (manual) / O export\n%s"),PhysicalSeconds,ActualRate,AppliedPressureDropPa,GasMassErrorKg,ParticleMassErrorKg,*Error));if(StopAt>0&&!ScreenshotTaken&&PhysicalSeconds>=StopAt*.6&&FParse::Param(FCommandLine::Get(),TEXT("CoupledScreenshot"))){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("CoupledPreview.png"),true,false);ScreenshotTaken=true;}if(StopAt>0&&(!Error.IsEmpty()||PhysicalSeconds>=StopAt)){ExportSnapshot();FPlatformMisc::RequestExitWithStatus(false,Error.IsEmpty()?0:1);StopAt=0;}}
void APowderCoupledTest::InjectPulse(){if(Runtime)Runtime->Pulse=true;}
void APowderCoupledTest::ExportSnapshot(){if(!Runtime)return;std::lock_guard<std::mutex> Lock(Runtime->Mutex);if(ExportDirectory.IsEmpty())ExportDirectory=FPaths::ProjectSavedDir()/TEXT("Coupled_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));IFileManager::Get().MakeDirectory(*ExportDirectory,true);
 std::ostringstream Csv;Csv<<std::setprecision(17)<<"id,type,x_m,y_m,z_m,radius_m\n";for(const auto& P:Runtime->Points)Csv<<P.Id<<','<<P.Type<<','<<P.Position.x<<','<<P.Position.y<<','<<P.Position.z<<','<<P.Radius<<'\n';bool SavedCsv=FFileHelper::SaveStringToFile(UTF8_TO_TCHAR(Csv.str().c_str()),*(ExportDirectory/TEXT("particles.csv")));
 const auto& A=Runtime->Report;FString Json=FString::Printf(TEXT("{\"time_s\":%.17g,\"rate\":%.17g,\"count\":%llu,\"gas_balance_kg\":%.17g,\"particle_balance_kg\":%.17g,\"exchange_error_Ns\":%.17g,\"max_overlap_m\":%.17g,\"min_epsilon\":%.17g,\"temperature_K\":%.17g,\"pressure_drop_Pa\":%.17g,\"dt_s\":%.17g,\"calibrated\":false,\"failed\":%s}"),A.time,Runtime->Wall>0?A.time/Runtime->Wall:0,static_cast<uint64>(A.count),A.gas_balance_error,A.particle_balance_error,A.exchange_error,A.max_overlap,A.min_epsilon,Runtime->AppliedTemperature,Runtime->AppliedDrop,Runtime->AppliedDt,Error.IsEmpty()?TEXT("false"):TEXT("true"));bool SavedJson=FFileHelper::SaveStringToFile(Json,*(ExportDirectory/TEXT("result.json")));FString Protocol=FString::Printf(TEXT("{\"experiment\":%d,\"supplied_kg\":%.17g,\"retained_kg\":%.17g,\"exited_kg\":%.17g,\"filter_contact_kg\":%.17g,\"minimum_accepted_dt_s\":%.17g,\"inlet_backflow_kg\":%.17g,\"outlet_kg\":%.17g,\"maximum_filter_contact_kg\":%.17g,\"minimum_filter_contact_after_02s_kg\":%.17g,\"contact_kn\":%.17g,\"contact_cn\":%.17g,\"wall_gamma\":%.17g,\"non_spherical_cpu\":true,\"two_way_air\":true,\"calibrated\":false}"),Runtime->Mode,Runtime->Supplied,A.particle_mass,Runtime->Exited,Runtime->Filter,Runtime->MinimumDt,Runtime->Backflow,Runtime->Outlet,Runtime->MaxFilter,Runtime->MinimumLateFilter,Runtime->ContactKn,Runtime->ContactCn,Runtime->WallGamma);Protocol.RemoveFromEnd(TEXT("}"));Protocol+=TEXT(",\"by_type\":")+FString(UTF8_TO_TCHAR(Runtime->SpeciesJson.c_str()))+TEXT("}");bool SavedProtocol=FFileHelper::SaveStringToFile(Protocol,*(ExportDirectory/TEXT("protocol.json")));if(!Error.IsEmpty())FFileHelper::SaveStringToFile(Error,*(ExportDirectory/TEXT("error.txt")));if(!SavedCsv||!SavedJson||!SavedProtocol){Error=TEXT("Coupled snapshot write failed");Running=false;Runtime->Run=false;}}
void APowderCoupledTest::EndPlay(const EEndPlayReason::Type Reason){Stop();Super::EndPlay(Reason);}
