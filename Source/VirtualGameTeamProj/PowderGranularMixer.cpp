#include "PowderGranularMixer.h"
#include "DEMParticleSettings.h"
#include "DEMEmitter.h"
#include <cmath>
#include <iomanip>
#include "powder/granular.hpp"
#include "powder/granular_coupled.hpp"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "UnrealClient.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <array>

using ProfileClock=std::chrono::steady_clock;
static double ProfileElapsed(ProfileClock::time_point Start){return std::chrono::duration<double>(ProfileClock::now()-Start).count();}
struct FGranularRuntime {
 std::atomic<bool> Quit{false},Run{true},Gate{false},Auto{true};std::atomic<double> Rpm{30},Rate{.1},MixEnd{9},PressureDrop{0};
 std::thread Worker;std::mutex Lock;std::vector<powder::Grain> Positions;std::string Error;double Time=0,Wall=0,Mix=0,Angle=0,Radius=0,ReceivedMass=0;int Receiver=0;uint64 Generation=0;
 // Display counters are game-thread-owned; written only after Worker has joined.
 bool Coupled=false;bool Stream=false;std::vector<powder::Grain> DisplayPoints;TArray<FTransform> TransformBuffers[2];
 double DisplayCopy=0,DisplayTransforms=0,DisplaySubmit=0;uint64 DisplayUpdates=0;
 std::filesystem::path ProfileDir;
 FGranularRuntime(powder::GranularConfig C,const FString& Folder,int Batch,double MassKg,std::vector<powder::Grain> Initial,std::vector<powder::Grain> Velocity,bool FromEmitters,bool StartRunning){std::filesystem::path Dir(*Folder);ProfileDir=Dir;Stream=FParse::Param(FCommandLine::Get(),TEXT("GrainStream"));
 Coupled=C.coupled_air;Run=StartRunning;Worker=std::thread([this,C,Dir,Batch,MassKg,Initial=std::move(Initial),Velocity=std::move(Velocity),FromEmitters]{try{
  std::unique_ptr<powder::GranularGPU> Owned=FromEmitters?std::make_unique<powder::GranularGPU>(C,Initial,Velocity):std::make_unique<powder::GranularGPU>(C);
  auto& Solver=*Owned;{std::lock_guard<std::mutex> Guard(Lock);Radius=Solver.radius();Positions=Solver.snapshot();++Generation;}std::ofstream Csv(Dir/"trace.csv");if(!Csv)throw std::runtime_error("cannot open trace");Csv<<std::setprecision(17);Csv<<"time_s,active_wall_s,rate,mix_index,receiver_count,total_count,mass_kg,gate,angle_rad\n";
  std::unique_ptr<powder::GranularGasCoupler> Gas;if(C.coupled_air){powder::GasConfig G;G.cells=C.test_mixer?std::array<int,3>{16,10,10}:std::array<int,3>{10,6,12};G.h=.1;G.boundary_pa={};Gas=std::make_unique<powder::GranularGasCoupler>(C,Positions,powder::GasGrid(G),C.test_mixer?powder::Vec3{-.8,-.5,-.5}:powder::Vec3{-.5,-.3,-.6});}
  auto WriteGasReport=[&]{if(Gas){std::ofstream GasReport(Dir/"coupled_gas.json");GasReport<<std::setprecision(17)<<"{\"enabled\":true,\"time_s\":"<<Gas->elapsed_s()<<",\"gas_mass_error_kg\":"<<Gas->gas_mass_error()<<",\"volume_error_m3\":"<<Gas->volume_error()<<",\"max_gas_speed\":"<<Gas->max_gas_speed()<<",\"simple_box_boundaries\":"<<(C.simple_box?"true":"false")<<",\"machine_gas_geometry_validated\":false,\"full_hertz_dmt\":false,\"non_spherical_supported\":false}";if(!GasReport)throw std::runtime_error("gas report write failed");}};
  {const auto& State=Positions;std::ofstream Bin(Dir/"initial.bin",std::ios::binary);Bin.write(reinterpret_cast<const char*>(State.data()),State.size()*sizeof(powder::Grain));if(!Bin)throw std::runtime_error("initial snapshot write failed");}
  {std::ofstream Air(Dir/"air_model.json");powder::write_granular_air(Air,C.air);if(!Air)throw std::runtime_error("air metadata write failed");}
  {std::ofstream Model(Dir/"material_model.json");Model<<std::setprecision(17)<<"{\"binary\":"<<(C.binary_materials?"true":"false")<<",\"rotation\":"<<(C.rotation?"true":"false")<<",\"rolling_friction\":"<<C.rolling_friction<<",\"cross_friction\":"<<C.cross_friction<<",\"contact_attraction_gamma_J_m2\":"<<C.cohesion_gamma_j_m2<<",\"wall_attraction_gamma_J_m2\":"<<C.wall_adhesion_gamma_j_m2<<",\"full_hertz_dmt\":false,\"calibrated\":false,\"materials\":[";for(int G=0;G<2;++G){auto M=C.materials[G];if(G)Model<<',';Model<<"{\"radius_m\":"<<M.radius_m<<",\"density_kg_m3\":"<<M.density_kg_m3<<",\"particle_mass_kg\":"<<M.mass_kg()<<",\"particle_friction\":"<<M.particle_friction<<",\"wall_friction\":"<<M.wall_friction<<"}";}Model<<"]}";if(!Model)throw std::runtime_error("model metadata write failed");}
  auto Last=std::chrono::steady_clock::now(),Start=Last,LastDisplay=Last;double Debt=0,Active=0,NextRecord=0,StatsWall=0,FileWall=0,PublishWall=0;
  std::vector<powder::Grain> Data;bool Pending=false,ReadGate=false;double ReadTime=0,ReadAngle=0,ReadWall=0;uint64 Reads=0,AuditIndex=0;double NextAudit=0;const bool AuditSnapshots=FParse::Param(FCommandLine::Get(),TEXT("GrainAuditSnapshots"));if(AuditSnapshots)std::filesystem::create_directories(Dir/"audit");
  auto Publish=[&](double T,double Theta,double SnapshotWall,bool Open){
   auto StatsBegin=ProfileClock::now();std::array<std::array<double,2>,64> Bins{};double A=0,InMixer=0,ReceivedKg=0;unsigned Received=0;for(const auto& P:Data){int X=FMath::Clamp(int((P.x+(C.test_mixer?.75f:.45f))/(C.test_mixer?1.5f:.9f)*4),0,3),Y=FMath::Clamp(int((P.y+(C.test_mixer?.45f:.25f))/(C.test_mixer?.9f:.5f)*4),0,3),Z=FMath::Clamp(int((P.z+(C.test_mixer?.325f:0))/(C.test_mixer?.65f:.4f)*4),0,3);if(C.test_mixer||P.z>=0){double M=C.binary_materials?C.materials[P.w==0?0:1].mass_kg():MassKg/C.count;InMixer+=M;Bins[(Z*4+Y)*4+X][P.w==0?0:1]+=M;if(P.w==0)A+=M;}if(!C.test_mixer&&P.z<-.25f){++Received;ReceivedKg+=C.binary_materials?C.materials[P.w==0?0:1].mass_kg():MassKg/C.count;}}
   double F=double(A)/FMath::Max(1.e-30,InMixer),Variance=0;for(auto B:Bins){double N=B[0]+B[1];if(N)Variance+=N/FMath::Max(1.e-30,InMixer)*std::pow(B[0]/N-F,2);}double I=(InMixer>0&&A>0&&A<InMixer)?1-Variance/FMath::Max(1.e-9,F*(1-F)):-1;
   StatsWall+=ProfileElapsed(StatsBegin);auto FileBegin=ProfileClock::now();
   if(T>=NextRecord){WriteGasReport();Csv<<T<<','<<SnapshotWall<<','<<T/SnapshotWall<<','<<I<<','<<Received<<','<<Data.size()<<','<<MassKg<<','<<Open<<','<<Theta<<'\n';Csv.flush();if(!Csv)throw std::runtime_error("trace write failed");std::ofstream Bin(Dir/"final.bin",std::ios::binary);Bin.write(reinterpret_cast<const char*>(Data.data()),Data.size()*sizeof(powder::Grain));if(!Bin)throw std::runtime_error("snapshot write failed");NextRecord=T+.25;
    if(AuditSnapshots&&T>=NextAudit){auto Stem=Dir/"audit"/std::to_string(AuditIndex++);std::ofstream Snapshot(Stem.string()+".bin",std::ios::binary);Snapshot.write(reinterpret_cast<const char*>(Data.data()),Data.size()*sizeof(powder::Grain));std::ofstream Meta(Stem.string()+".json");Meta<<std::setprecision(17)<<"{\"time_s\":"<<T<<",\"angle_rad\":"<<Theta<<",\"mix_index\":"<<I<<"}";if(!Snapshot||!Meta)throw std::runtime_error("audit snapshot write failed");NextAudit=T+2;}
   }
   FileWall+=ProfileElapsed(FileBegin);auto PublishBegin=ProfileClock::now();
   {std::lock_guard<std::mutex> Guard(Lock);if(Stream)Positions.swap(Data);else Positions=std::move(Data);Time=T;Wall=SnapshotWall;Mix=I;Angle=Theta;Receiver=static_cast<int>(Received);ReceivedMass=ReceivedKg;++Generation;}
   PublishWall+=ProfileElapsed(PublishBegin);
  };
  while(!Quit){
   if(Stream&&Pending&&Solver.receive_snapshot(Data)){Publish(ReadTime,ReadAngle,ReadWall,ReadGate);Pending=false;}
   auto Now=std::chrono::steady_clock::now();double Delta=std::chrono::duration<double>(Now-Last).count();Last=Now;
   if(!Run){Debt=0;std::this_thread::sleep_for(std::chrono::milliseconds(3));continue;}
   // Bound unserved wall-time demand; a lower target must not repay an unlimited backlog.
   Active+=Delta;Debt=FMath::Min(Debt+Delta*Rate.load(),2.*double(Batch)*double(C.dt));unsigned Steps=static_cast<unsigned>(std::min(double(Batch),std::floor(Debt/C.dt)));
   if(!Steps){std::this_thread::sleep_for(std::chrono::milliseconds(1));continue;}
   double T=Solver.time();bool Open=!C.test_mixer&&(Auto?(T>=MixEnd):Gate.load());float Speed=static_cast<float>((Auto&&T<1)?0:Rpm.load());if(Gas){if(C.simple_box){double Drop=PressureDrop.load();if(!std::isfinite(Drop)||std::abs(Drop)>5000)throw std::runtime_error("simple box pressure drop outside +/-5000 Pa");Gas->gas.config.boundary_pa[4]=101325;Gas->gas.config.boundary_pa[5]=101325-Drop;}Gas->advance(Solver,Steps,Speed,Open);}else Solver.advance(Steps,Speed,Open);Debt=FMath::Max(0.,Debt-double(Steps)*C.dt);
   Now=std::chrono::steady_clock::now();if(Pending||std::chrono::duration<double>(Now-LastDisplay).count()<.05)continue;LastDisplay=Now;++Reads;
   if(Stream){ReadTime=Solver.time();ReadAngle=Solver.angle();ReadWall=Active;ReadGate=Open;Solver.request_snapshot();Pending=true;}
   else{Data=Solver.snapshot();Publish(Solver.time(),Solver.angle(),Active,Open);}
  }
  if(Pending&&Solver.receive_snapshot(Data,true))Publish(ReadTime,ReadAngle,ReadWall,ReadGate);
  WriteGasReport();
  std::ofstream Timing(Dir/"timing.json");powder::write_granular_timing(Timing,Solver.timing());if(!Timing)throw std::runtime_error("timing write failed");
  std::ofstream Host(Dir/"worker_timing.json");Host<<std::setprecision(12)<<"{\"statistics_cpu_wall_s\":"<<StatsWall<<",\"file_io_wall_s\":"<<FileWall<<",\"publish_lock_wall_s\":"<<PublishWall<<",\"stream_enabled\":"<<(Stream?"true":"false")<<",\"readbacks\":"<<Reads<<",\"solver_time_s\":"<<Solver.time()<<"}";
 }catch(const std::exception& E){std::lock_guard<std::mutex> Guard(Lock);Error=E.what();}});
 }
 ~FGranularRuntime(){Quit=true;if(Worker.joinable())Worker.join();std::ofstream Out(ProfileDir/"display_timing.json");Out<<std::setprecision(12)<<"{\"updates\":"<<DisplayUpdates<<",\"copy_and_lock_cpu_wall_s\":"<<DisplayCopy<<",\"transform_cpu_wall_s\":"<<DisplayTransforms<<",\"ism_submission_cpu_wall_s\":"<<DisplaySubmit<<",\"render_gpu_measured\":false}";}
};
APowderGranularMixer::APowderGranularMixer(){PrimaryActorTick.bCanEverTick=true;auto* Root=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));RootComponent=Root;
 Shaft=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shaft"));Shaft->SetupAttachment(Root);Shaft->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));Shaft->SetRelativeLocation(FVector(0,0,21));Shaft->SetRelativeRotation(FRotator(0,0,90));Shaft->SetRelativeRotation(FQuat(FVector::YAxisVector,PI/2));Shaft->SetRelativeScale3D(FVector(.05,.05,.9));Shaft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 GateMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OutletGate"));GateMesh->SetupAttachment(Root);GateMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));GateMesh->SetRelativeLocation(FVector(0,0,-1));GateMesh->SetRelativeScale3D(FVector(.12,.12,.02));GateMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 GroupA=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RedGrains"));GroupB=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BlueGrains"));Walls=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CutawayVessel"));Blades=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RotatingPaddles"));
 for(auto* C:{GroupA.Get(),GroupB.Get(),Walls.Get(),Blades.Get()}){C->SetupAttachment(Root);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetMobility(EComponentMobility::Movable);C->SetCastShadow(false);C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,C==GroupA||C==GroupB?TEXT("/Engine/BasicShapes/Sphere.Sphere"):TEXT("/Engine/BasicShapes/Cube.Cube")));}}
APowderGranularMixer::~APowderGranularMixer(){Stop();}
void APowderGranularMixer::Stop(){delete Runtime;Runtime=nullptr;for(auto E:ActiveEmitters)if(E.IsValid())E->ReleaseGPU(this);ActiveEmitters.Empty();ActiveSourceCounts.Empty();}
void APowderGranularMixer::Geometry(){
 if(CoupledSimpleBox){Shaft->SetVisibility(false);GateMesh->SetVisibility(false);Walls->ClearInstances();Blades->ClearInstances();Walls->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));auto Add=[&](FVector P,FVector S){Walls->AddInstance(FTransform(FQuat::Identity,P,S));};Add(FVector(0,0,-51),FVector(1.6,1,.02));Add(FVector(0,51,0),FVector(1.6,.02,1));Add(FVector(-81,0,0),FVector(.02,1,1));Add(FVector(81,0,0),FVector(.02,1,1));auto* Grain=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/DEM/SM_GrainSphere.SM_GrainSphere"));if(Grain){GroupA->SetStaticMesh(Grain);GroupB->SetStaticMesh(Grain);}auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/M_BulkFlow.M_BulkFlow"));int K=0;for(auto* C:{GroupA.Get(),GroupB.Get(),Walls.Get()}){if(Material)C->SetMaterial(0,Material);if(auto* M=C->CreateDynamicMaterialInstance(0))M->SetVectorParameterValue(TEXT("Tint"),K==0?FLinearColor(.8,.04,.02):K==1?FLinearColor(.02,.3,.9):FLinearColor(.3,.35,.4));++K;}return;}
 if(TestMixerAsset){Walls->ClearInstances();Blades->ClearInstances();Shaft->SetVisibility(false);GateMesh->SetVisibility(false);
  Walls->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/DEM/SM_MX301_TestVessel.SM_MX301_TestVessel")));Blades->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/DEM/SM_MX301_TestRotor.SM_MX301_TestRotor")));Walls->AddInstance(FTransform::Identity);Blades->AddInstance(FTransform::Identity);
  auto* Grain=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/DEM/SM_GrainSphere.SM_GrainSphere"));if(Grain){GroupA->SetStaticMesh(Grain);GroupB->SetStaticMesh(Grain);}auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/M_BulkFlow.M_BulkFlow"));int K=0;for(auto* C:{GroupA.Get(),GroupB.Get(),Walls.Get(),Blades.Get()}){if(Material)C->SetMaterial(0,Material);if(auto* M=C->CreateDynamicMaterialInstance(0))M->SetVectorParameterValue(TEXT("Tint"),K==0?FLinearColor(.8,.04,.02):K==1?FLinearColor(.02,.3,.9):FLinearColor(.4,.45,.5));++K;}return;}
 Shaft->SetVisibility(true);Walls->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Blades->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
auto* GrainMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/DEM/SM_GrainSphere.SM_GrainSphere"));if(GrainMesh){GroupA->SetStaticMesh(GrainMesh);GroupB->SetStaticMesh(GrainMesh);}Walls->ClearInstances();Blades->ClearInstances();auto Add=[&](FVector P,FVector S){Walls->AddInstance(FTransform(FQuat::Identity,P,S));};
 // Four floor strips leave the physical 12 cm square opening visible.
 Add(FVector(-25.5,0,-1),FVector(.39,.5,.02));Add(FVector(25.5,0,-1),FVector(.39,.5,.02));Add(FVector(0,-15.5,-1),FVector(.12,.19,.02));Add(FVector(0,15.5,-1),FVector(.12,.19,.02));
 Add(FVector(0,26,20),FVector(.9,.02,.4));Add(FVector(-46,0,20),FVector(.02,.5,.4));Add(FVector(46,0,20),FVector(.02,.5,.4));
 Add(FVector(0,0,-61),FVector(.9,.5,.02));Add(FVector(0,26,-42.5),FVector(.9,.02,.35));Add(FVector(-46,0,-42.5),FVector(.02,.5,.35));Add(FVector(46,0,-42.5),FVector(.02,.5,.35));
 Add(FVector(0,7,-12.5),FVector(.12,.02,.25));Add(FVector(-7,0,-12.5),FVector(.02,.12,.25));Add(FVector(7,0,-12.5),FVector(.02,.12,.25));
 for(int K=0;K<6;K++)Blades->AddInstance(FTransform());
 UInstancedStaticMeshComponent* Components[]={GroupA,GroupB,Walls,Blades};FLinearColor Colors[]={FLinearColor(.8f,.04f,.02f),FLinearColor(.02f,.3f,.9f),FLinearColor(.12f,.15f,.19f),FLinearColor(.7f,.72f,.75f)};auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/M_BulkFlow.M_BulkFlow"));for(int I=0;I<4;I++){if(Material)Components[I]->SetMaterial(0,Material);if(auto* M=Components[I]->CreateDynamicMaterialInstance(0))M->SetVectorParameterValue(TEXT("Tint"),Colors[I]);}}
void APowderGranularMixer::OnConstruction(const FTransform& T){Super::OnConstruction(T);Geometry();}
int32 APowderGranularMixer::ParticleCountForMass(UDEMParticleSettings* Settings,double MassKg)const{
 if(!Settings||!std::isfinite(MassKg)||MassKg<=0)return -1;FDEMParticleDerivedProperties P;TArray<FText> Errors;if(!Settings->GetDerivedProperties(P,Errors))return -1;double N=std::floor(MassKg/P.MassKg+.5);return N>=1&&N<=1000000?int32(N):-1;
}
powder::GranularAir APowderGranularMixer::GetAirSettings() const {
 powder::GranularAir A;A.enabled=EnablePrescribedAir;A.density_kg_m3=AirDensityKgM3;A.viscosity_pa_s=AirViscosityPaS;
 A.velocity_m_s={AirVelocityMetersPerSecond.X,AirVelocityMetersPerSecond.Y,AirVelocityMetersPerSecond.Z};A.validate();
 if(A.enabled&&!BinaryMaterials)throw std::invalid_argument("Prescribed air requires Binary Materials with explicit density.");
 return A;
}
bool APowderGranularMixer::ValidateSimulationSettings(){
 if(Runtime){SettingsStatus=TEXT("Simulation active; use Reset to apply settings.");return false;}
 TArray<FText> Errors;
 try{GetAirSettings();}catch(const std::exception& E){Errors.Add(FText::FromString(UTF8_TO_TCHAR(E.what())));}
 if(!BinaryMaterials&&ParticleSettings&&ParticleSettings->ParticleFriction!=ParticleSettings->WallFriction)Errors.Add(FText::FromString(TEXT("Distinct wall friction requires Binary Materials.")));
 if(BinaryMaterials&&!UseEmitters)Errors.Add(FText::FromString(TEXT("Binary Materials requires explicit emitters.")));
 if(EnableParticleRotation&&!BinaryMaterials)Errors.Add(FText::FromString(TEXT("Particle rotation requires Binary Materials.")));
 if(!std::isfinite(RollingFriction)||RollingFriction<0||RollingFriction>1)Errors.Add(FText::FromString(TEXT("Rolling friction must be in [0,1].")));
 if(ParticleCount<1||ParticleCount>1000000) Errors.Add(FText::FromString(TEXT("ParticleCount must be 1..1000000.")));
 if(ContactIterations<2||ContactIterations>32||!std::isfinite(TimeStep)||TimeStep<=0||TimeStep>.001||static_cast<float>(TimeStep)<=0) Errors.Add(FText::FromString(TEXT("Iterations must be 2..32 and dt in (0,0.001].")));
 double Density=DensityKgPerCubicMeter;
 float Radius=ParticleCount>0?static_cast<float>(std::cbrt(.054/(ParticleCount*4.188790204786391))):0;
 double Mu=Friction;
 if(ParticleSettings){TArray<FText> AssetErrors;ParticleSettings->ValidateGPU(AssetErrors);Errors.Append(AssetErrors);Radius=static_cast<float>(ParticleSettings->RadiusMeters);Density=ParticleSettings->DensityKgPerCubicMeter;Mu=ParticleSettings->ParticleFriction;}
 if(!std::isfinite(Radius)||Radius<=.001f||Radius>=.03f||!std::isfinite(Density)||Density<=0||!std::isfinite(Mu)||Mu<0||Mu>1) Errors.Add(FText::FromString(TEXT("Invalid GPU radius, density or friction.")));
 const double Mass=Density*4.188790204786391*double(Radius)*Radius*Radius;
 if(!std::isnormal(Mass)||!std::isnormal(Mass*ParticleCount)) Errors.Add(FText::FromString(TEXT("Mass calculation overflow/underflow.")));
 SettingsStatus.Empty();for(const auto& E:Errors)SettingsStatus+=E.ToString()+TEXT("; ");
 if(!Errors.IsEmpty()){ChargeMassKg=0;RadiusMm=0;return false;}
 ChargeMassKg=Mass*ParticleCount;RadiusMm=Radius*1000.;
 SettingsStatus=TEXT("GPU approximate spheres: density is accounting only; stiffness/damping/spin unsupported. Changes require Reset.");return true;
}
bool APowderGranularMixer::BuildEmitterInitialState(std::vector<powder::Grain>& Positions,std::vector<powder::Grain>& Velocities){
 Positions.clear();Velocities.clear();Error.Empty();
 auto Fail=[&](const FString& Why){Positions.clear();Velocities.clear();Error=Why;return false;};
 if(Runtime)return Fail(TEXT("Stop/reset before collecting emitter data."));
 const FTransform Frame=GetActorTransform();
 if(Frame.ContainsNaN()||!Frame.GetScale3D().Equals(FVector::OneVector,1.e-8)||!Frame.GetRotation().RotateVector(FVector::UpVector).Equals(FVector::UpVector,1.e-6))
  return Fail(TEXT("Mixer requires unit scale and upright local Z (translation/yaw supported)."));
 if(Emitters.IsEmpty())return Fail(TEXT("Use Emitters requires an explicit nonempty source list."));
 TSet<ADEMEmitter*> Seen;UDEMParticleSettings* Common=nullptr;UDEMParticleSettings* Groups[2]={nullptr,nullptr};int64 Total=0;EmitterConfig={};EmitterConfig.binary_materials=BinaryMaterials;EmitterConfig.cross_friction=float(CrossMaterialFriction);EmitterConfig.rotation=EnableParticleRotation;EmitterConfig.rolling_friction=float(RollingFriction);
 for(auto E:Emitters){
  if(!IsValid(E)||E->GetWorld()!=GetWorld()||Seen.Contains(E))return Fail(TEXT("Invalid, foreign-world or duplicate emitter."));Seen.Add(E);if(E->GetGPUOwner())return Fail(TEXT("Emitter is already owned by a running mixer."));
  if(E->GPUGroup<0||E->GPUGroup>1)return Fail(TEXT("GPUGroup must be 0 (red) or 1 (blue)."));
  if(!E->ParticleSettings)return Fail(TEXT("Emitter has no particle settings."));
  if(!BinaryMaterials&&E->ParticleSettings->ParticleFriction!=E->ParticleSettings->WallFriction)return Fail(TEXT("Distinct wall friction requires Binary Materials."));
  TArray<FText> Errors;if(!E->ParticleSettings->ValidateGPU(Errors)){FString Message;for(auto& X:Errors)Message+=X.ToString()+TEXT("; ");return Fail(Message);}
  auto Matches=[](const UDEMParticleSettings* A,const UDEMParticleSettings* B){return A->RadiusMeters==B->RadiusMeters&&A->DensityKgPerCubicMeter==B->DensityKgPerCubicMeter&&A->ParticleFriction==B->ParticleFriction&&A->WallFriction==B->WallFriction;};
  if(!Common)Common=E->ParticleSettings;else if(!BinaryMaterials&&!Matches(Common,E->ParticleSettings))return Fail(TEXT("All emitters require identical GPU radius, density and friction."));
  if(!BinaryMaterials&&ParticleSettings&&(!ParticleSettings->bGPUApproximation||!Matches(Common,ParticleSettings)))return Fail(TEXT("Mixer settings disagree with emitter settings; clear or match the mixer asset."));
  if(Groups[E->GPUGroup]&&!Matches(Groups[E->GPUGroup],E->ParticleSettings))return Fail(TEXT("Binary mode requires identical properties within each group."));
  Groups[E->GPUGroup]=E->ParticleSettings;
  if(E->ParticleCount<0||E->ParticleCount>ADEMEmitter::MaxParticleCount)return Fail(TEXT("Emitter particle count out of range."));
  Total+=E->ParticleCount;if(Total>1000000)return Fail(TEXT("Total emitter count exceeds 1000000."));
 }
 if(Total==0)return Fail(TEXT("At least one emitted particle is required."));
 for(int G=0;G<2;++G){auto* S=Groups[G]?Groups[G]:Common;auto& M=EmitterConfig.materials[G];M.source_diameter_m=S->SourceDiameterMeters;M.radius_m=S->RadiusMeters;M.density_kg_m3=S->DensityKgPerCubicMeter;M.particle_friction=S->ParticleFriction;M.wall_friction=S->WallFriction;}
 if(BinaryMaterials&&(!std::isfinite(CrossMaterialFriction)||CrossMaterialFriction<0||CrossMaterialFriction>1))return Fail(TEXT("Cross friction must be finite in [0,1]."));
 try{EmitterConfig.air=GetAirSettings();if(EmitterConfig.air.enabled)for(const auto& M:EmitterConfig.materials)EmitterConfig.air.validate_material(M);}catch(const std::exception& E){return Fail(UTF8_TO_TCHAR(E.what()));}
 Positions.reserve(Total);Velocities.reserve(Total);
 for(auto E:Emitters){
  if(!E->TryGenerate())return Fail(E->GenerationStatus);
  FDEMEmitterBurst Burst;if(!E->CopyGeneratedBurst(Burst))return Fail(TEXT("Emitter did not produce a valid burst."));
  for(const auto& P:Burst.Particles){
   // Source data is reflected right-handed SI world space; GPU uses UE local axes in meters.
   const FVector WorldCM=FVector(P.PositionMeters.X,-P.PositionMeters.Y,P.PositionMeters.Z)*100.;
   const FVector Local=Frame.InverseTransformPosition(WorldCM)*.01;
   const FVector V=Frame.InverseTransformVectorNoScale(FVector(P.LinearVelocityMetersPerSecond.X,-P.LinearVelocityMetersPerSecond.Y,P.LinearVelocityMetersPerSecond.Z));
   if(Local.ContainsNaN()||V.ContainsNaN()||!P.AngularVelocityRadiansPerSecond.IsZero())return Fail(TEXT("Invalid position/velocity or unsupported spin."));
   // Conservative initial domain check; detailed vessel/rotor/particle overlap checks are step 05.
   if(FMath::Abs(Local.X)>=(TestMixerAsset?.8:.46)||FMath::Abs(Local.Y)>=(TestMixerAsset?.5:.26)||Local.Z< (TestMixerAsset?-.5:0.)||Local.Z> (TestMixerAsset?.5:.4))return Fail(TEXT("Emitter lies outside the supported initial GPU domain."));
   powder::Grain A{float(Local.X),float(Local.Y),float(Local.Z),float(E->GPUGroup)},B{float(V.X),float(V.Y),float(V.Z),0};
   if(!std::isfinite(B.x)||!std::isfinite(B.y)||!std::isfinite(B.z))return Fail(TEXT("Initial velocity cannot be represented as GPU float."));
   Positions.push_back(A);Velocities.push_back(B);
  }
 }
 powder::GranularConfig Check=EmitterConfig;Check.count=static_cast<unsigned>(Total);Check.radius=float(Common->RadiusMeters);Check.test_mixer=TestMixerAsset;Check.simple_box=CoupledSimpleBox;
 try{powder::validate_granular_initial(Check,Positions);}catch(const std::exception& E){return Fail(UTF8_TO_TCHAR(E.what()));}
 ParticleCount=static_cast<int32>(Total);ParticleSettings=Common;return true;
}
std::vector<powder::Grain> APowderGranularMixer::CopyParticleState(){if(!Runtime)return {};std::lock_guard<std::mutex> Guard(Runtime->Lock);return Runtime->Positions;}
void APowderGranularMixer::ResetSimulation(){Stop();GroupA->ClearInstances();GroupB->ClearInstances();Generation=0;Error.Empty();PhysicalSeconds=0;FinishWall=0;Shot1=false;Shot2=false;ReceiverCount=0;ReceiverMassKg=0;ActualRate=0;MixingIndex=0;ActiveParticleMassKg=0;PendingEmitterStart=false;SourceResetRequired=false;std::vector<powder::Grain> Initial,Velocity;if(UseEmitters&&!BuildEmitterInitialState(Initial,Velocity)){ChargeMassKg=0;return;}if(!ValidateSimulationSettings()){Error=SettingsStatus;return;}
 if(UseEmitters&&BinaryMaterials){ChargeMassKg=0;for(const auto& P:Initial)ChargeMassKg+=EmitterConfig.materials[P.w==0?0:1].mass_kg();RadiusMm=1000*std::max(EmitterConfig.materials[0].radius_m,EmitterConfig.materials[1].radius_m);SettingsStatus=TEXT("Binary approximate spheres: mass-weighted contacts; optional rotation; no cohesion or pharmaceutical calibration.");}
 if(EnablePrescribedAir)SettingsStatus+=TEXT(" AIR: prescribed uniform isolated-sphere drag/buoyancy; no CFD, dense shielding or vacuum prediction. Reset applies changes.");
 if(!HasActorBegunPlay())return;
 OutputDir=FPaths::ProjectSavedDir()/(TEXT("Granular_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))+TEXT("_")+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));FString Override;if(FParse::Value(FCommandLine::Get(),TEXT("GrainOutput="),Override))OutputDir=Override;IFileManager::Get().MakeDirectory(*OutputDir,true);
 powder::GranularConfig C=UseEmitters?EmitterConfig:powder::GranularConfig{};C.cohesion_gamma_j_m2=float(GPUCohesionGamma);C.wall_adhesion_gamma_j_m2=float(GPUWallAdhesionGamma);C.coupled_air=EnableCoupledGridAir;C.simple_box=CoupledSimpleBox;C.count=ParticleCount;C.iterations=ContactIterations;C.dt=static_cast<float>(TimeStep);C.test_mixer=TestMixerAsset;C.kernel_optimize=FParse::Param(FCommandLine::Get(),TEXT("GrainKernelOptimize"));C.boundary_candidates=FParse::Param(FCommandLine::Get(),TEXT("GrainBoundaryCandidates"));C.profile_split=FParse::Param(FCommandLine::Get(),TEXT("GrainProfileSplit"));C.profile=C.profile_split||FParse::Param(FCommandLine::Get(),TEXT("GrainProfile"));C.candidate_reuse=FParse::Param(FCommandLine::Get(),TEXT("GrainCandidateReuse"));C.candidates=C.candidate_reuse||FParse::Param(FCommandLine::Get(),TEXT("GrainCandidates"));FParse::Value(FCommandLine::Get(),TEXT("GrainCandidateSkinRatio="),C.candidate_skin_ratio);FParse::Value(FCommandLine::Get(),TEXT("GrainCandidateCapacity="),C.candidate_capacity);C.friction=static_cast<float>(ParticleSettings?ParticleSettings->ParticleFriction:Friction);C.radius=static_cast<float>(RadiusMm/1000.);const bool ValidatedDefaults=!C.binary_materials&&UseEmitters&&C.test_mixer&&C.count==50000&&C.iterations==16&&FMath::Abs(C.dt-.000375f)<1.e-9f&&FMath::Abs(C.radius-.0063647064f)<1.e-8f&&FMath::Abs(C.friction-.4f)<1.e-6f&&!FParse::Param(FCommandLine::Get(),TEXT("GrainLegacy"));C.kernel_optimize=C.kernel_optimize||ValidatedDefaults||(C.binary_materials&&MaterialOptimizations);C.candidate_reuse=C.candidate_reuse||ValidatedDefaults||(C.binary_materials&&MaterialOptimizations);C.candidates=C.candidates||C.candidate_reuse;ActiveParticleMassKg=ChargeMassKg/ParticleCount;Runtime=new FGranularRuntime(C,OutputDir,FMath::Clamp(SolverBatchSize,1,128),ChargeMassKg,std::move(Initial),std::move(Velocity),UseEmitters,Running);if(UseEmitters)for(auto E:Emitters){if(!E->BindGPU(this)){Error=TEXT("Emitter ownership failed; Reset required.");Stop();return;}ActiveEmitters.Add(E);ActiveSourceCounts.Add(E->ParticleCount);}ActiveMixerFrame=GetActorTransform();Runtime->Run=Running;Runtime->Rate=TargetRate;Runtime->Rpm=RPM;Runtime->Auto=AutomaticSequence;Runtime->MixEnd=MixUntilSeconds;}
void APowderGranularMixer::BeginPlay(){Super::BeginPlay();FParse::Value(FCommandLine::Get(),TEXT("GrainCohesionGamma="),GPUCohesionGamma);FParse::Value(FCommandLine::Get(),TEXT("GrainWallAdhesionGamma="),GPUWallAdhesionGamma);if(FParse::Param(FCommandLine::Get(),TEXT("GrainCoupledBox"))){CoupledSimpleBox=true;TestMixerAsset=true;}if(FParse::Param(FCommandLine::Get(),TEXT("GrainCoupledGridAir")))EnableCoupledGridAir=true;if(FParse::Param(FCommandLine::Get(),TEXT("GrainPrescribedAir"))){EnablePrescribedAir=true;FParse::Value(FCommandLine::Get(),TEXT("GrainAirX="),AirVelocityMetersPerSecond.X);FParse::Value(FCommandLine::Get(),TEXT("GrainAirY="),AirVelocityMetersPerSecond.Y);FParse::Value(FCommandLine::Get(),TEXT("GrainAirZ="),AirVelocityMetersPerSecond.Z);}Geometry();Benchmark=FParse::Param(FCommandLine::Get(),TEXT("GrainBenchmark"));if(Benchmark){FParse::Value(FCommandLine::Get(),TEXT("GrainCount="),ParticleCount);if(!FParse::Param(FCommandLine::Get(),TEXT("GrainSavedRate")))TargetRate=2;FParse::Value(FCommandLine::Get(),TEXT("GrainDt="),TimeStep);FParse::Value(FCommandLine::Get(),TEXT("GrainIterations="),ContactIterations);FParse::Value(FCommandLine::Get(),TEXT("GrainBatch="),SolverBatchSize);FParse::Value(FCommandLine::Get(),TEXT("GrainSeconds="),BenchmarkSeconds);if(FParse::Param(FCommandLine::Get(),TEXT("GrainTestMixer")))TestMixerAsset=true;}Geometry();if(UseEmitters)PendingEmitterStart=true;else ResetSimulation();auto* Camera=GetWorld()->SpawnActor<ACameraActor>();Camera->SetActorLocation(GetActorTransform().TransformPosition((TestMixerAsset?FVector(170,-210,115):FVector(115,-160,95))));Camera->SetActorRotation((GetActorTransform().TransformPosition(FVector(0,0,-5))-Camera->GetActorLocation()).Rotation());if(auto* PC=UGameplayStatics::GetPlayerController(this,0))PC->SetViewTarget(Camera);}
void APowderGranularMixer::EndPlay(const EEndPlayReason::Type Reason){Stop();Super::EndPlay(Reason);}
void APowderGranularMixer::Display(){if(!Runtime)return;auto CopyBegin=ProfileClock::now();std::unique_lock<std::mutex> Guard(Runtime->Lock);Error=UTF8_TO_TCHAR(Runtime->Error.c_str());if(Generation==Runtime->Generation)return;Generation=Runtime->Generation;PhysicalSeconds=Runtime->Time;ActualRate=Runtime->Wall>0?PhysicalSeconds/Runtime->Wall:0;RadiusMm=Runtime->Radius*1000;MixingIndex=Runtime->Mix;ReceiverCount=Runtime->Receiver;ReceiverMassKg=Runtime->ReceivedMass;std::vector<powder::Grain> LocalPoints;if(Runtime->Stream)Runtime->DisplayPoints.assign(Runtime->Positions.begin(),Runtime->Positions.end());else LocalPoints=Runtime->Positions;const auto& Points=Runtime->Stream?Runtime->DisplayPoints:LocalPoints;double DisplayAngle=Runtime->Angle,DisplayRadius=Runtime->Radius;Guard.unlock();Runtime->DisplayCopy+=ProfileElapsed(CopyBegin);++Runtime->DisplayUpdates;
 if(!ActiveEmitters.IsEmpty()){
  int32 Offset=0;for(int32 K=0;K<ActiveEmitters.Num();++K){auto TransformBegin=ProfileClock::now();TArray<FTransform> LocalData;auto& Data=Runtime->Stream?Runtime->TransformBuffers[0]:LocalData;Data.Reset();Data.Reserve(ActiveSourceCounts[K]);for(int32 I=0;I<ActiveSourceCounts[K];++I){auto& P=Points[Offset++];Data.Emplace(FQuat::Identity,ActiveMixerFrame.TransformPosition(FVector(P.x,P.y,P.z)*100.));}Runtime->DisplayTransforms+=ProfileElapsed(TransformBegin);auto SubmitBegin=ProfileClock::now();if(ActiveEmitters[K].IsValid())ActiveEmitters[K]->DisplayGPU(Data);Runtime->DisplaySubmit+=ProfileElapsed(SubmitBegin);}
 }else{
 auto TransformBegin=ProfileClock::now();TArray<FTransform> LocalData[2];auto* Data=Runtime->Stream?Runtime->TransformBuffers:LocalData;Data[0].Reset();Data[1].Reset();for(const auto& P:Points)Data[P.w==0?0:1].Add(FTransform(FQuat::Identity,FVector(P.x,P.y,P.z)*100,FVector(DisplayRadius*2)));Runtime->DisplayTransforms+=ProfileElapsed(TransformBegin);auto SubmitBegin=ProfileClock::now();UInstancedStaticMeshComponent* Groups[]={GroupA,GroupB};for(int K=0;K<2;K++){if(Groups[K]->GetInstanceCount()!=Data[K].Num()){Groups[K]->ClearInstances();Groups[K]->AddInstances(Data[K],false,false);}else Groups[K]->BatchUpdateInstancesTransforms(0,Data[K],false,true,true);}Runtime->DisplaySubmit+=ProfileElapsed(SubmitBegin);
 }
 if(CoupledSimpleBox)return;
 if(TestMixerAsset){Blades->UpdateInstanceTransform(0,FTransform(FQuat(FVector::ForwardVector,DisplayAngle)),false,true,true);return;}
 for(int K=0;K<6;K++){double A=DisplayAngle+K*PI/3;FQuat Q(FVector::ForwardVector,A);Blades->UpdateInstanceTransform(K,FTransform(Q*FQuat(FVector::UpVector,K%2?-.4:.4),FVector(-37.5+K*15,0,21)+Q.RotateVector(FVector(0,9.5,0)),FVector(.09,.14,.024)),false,K==5,true);}
}
void APowderGranularMixer::Tick(float Delta){Super::Tick(Delta);if(PendingEmitterStart)ResetSimulation();if(!Runtime){
 if(!Error.IsEmpty()&&GEngine)GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this),0,FColor::Red,Error);
 if(Benchmark&&!Error.IsEmpty()){FString Dir;FParse::Value(FCommandLine::Get(),TEXT("GrainOutput="),Dir);if(!Dir.IsEmpty()){IFileManager::Get().MakeDirectory(*Dir,true);FString Safe=Error.Replace(TEXT("\""),TEXT("'"));FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"count\":%d,\"time\":0,\"rate\":0,\"error\":\"%s\"}"),ParticleCount,*Safe),*(Dir/TEXT("result.json")));}FPlatformMisc::RequestExit(false);}
 return;}

 if(!ActiveEmitters.IsEmpty()){
  bool Valid=UseEmitters&&ActiveEmitters.Num()==Emitters.Num()&&GetActorTransform().Equals(ActiveMixerFrame);
  for(int32 I=0;I<ActiveEmitters.Num()&&Valid;++I)Valid=ActiveEmitters[I].IsValid()&&Emitters[I]==ActiveEmitters[I].Get()&&ActiveEmitters[I]->IsGPUValid(this);
  if(!Valid)SourceResetRequired=true;
  if(SourceResetRequired){Running=false;Runtime->Run=false;Error=TEXT("Emitter or mixer changed/removed; Reset required.");if(GEngine)GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this),0,FColor::Red,Error);return;}
 }
 Runtime->PressureDrop=CoupledPressureDropPa;Runtime->Run=Running;Runtime->Rate=FMath::Clamp(TargetRate,.01,2.);Runtime->Rpm=FMath::Clamp(RPM,0.,60.);Runtime->Gate=GateOpen;Runtime->Auto=AutomaticSequence;Runtime->MixEnd=MixUntilSeconds;Display();GateMesh->SetVisibility(!TestMixerAsset&&!(AutomaticSequence?PhysicalSeconds>=MixUntilSeconds:GateOpen));Phase=AutomaticSequence?(PhysicalSeconds<1?TEXT("settle"):PhysicalSeconds<MixUntilSeconds?TEXT("mix"):TEXT("discharge")):(GateOpen?TEXT("gate open"):TEXT("gate closed"));
 if(CoupledSimpleBox)Phase=FString::Printf(TEXT("SIMPLE BOX | %.3fkg"),ChargeMassKg);else if(TestMixerAsset)Phase=FString::Printf(TEXT("MX301 TEST ONLY | closed mixing | %.3fkg"),ChargeMassKg);
 if(Runtime->Coupled)Phase+=CoupledSimpleBox?TEXT(" | TWO-WAY AIR / SIMPLE BOX / VIRTUAL MATERIALS"):TEXT(" | TWO-WAY AIR: rectangular domain experiment, no gas machine boundary validation");
 if(GEngine)GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this),0,FColor::Cyan,FString::Printf(TEXT("GPU granular APPROX | %s | N %d r %.2fmm | t %.2fs %.3fx | I %.3f receiver %d | %s"),*Phase,ParticleCount,RadiusMm,PhysicalSeconds,ActualRate,MixingIndex,ReceiverCount,*Error));
 if(Benchmark){if(PhysicalSeconds>3&&!Shot1){FScreenshotRequest::RequestScreenshot(OutputDir/TEXT("mix.png"),true,false);Shot1=true;}if(PhysicalSeconds>10&&!Shot2){FScreenshotRequest::RequestScreenshot(OutputDir/TEXT("discharge.png"),true,false);Shot2=true;}if((PhysicalSeconds>=BenchmarkSeconds||!Error.IsEmpty())&&FinishWall==0){Running=false;FinishWall=FPlatformTime::Seconds();FString J=FString::Printf(TEXT("{\"count\":%d,\"time\":%.9f,\"rate\":%.9f,\"receiver\":%d,\"error\":\"%s\",\"physical_accuracy_validated\":false}"),ParticleCount,PhysicalSeconds,ActualRate,ReceiverCount,*Error);FFileHelper::SaveStringToFile(J,*(OutputDir/TEXT("result.json")));}if(FinishWall>0&&FPlatformTime::Seconds()-FinishWall>1)FPlatformMisc::RequestExit(false);}
}

