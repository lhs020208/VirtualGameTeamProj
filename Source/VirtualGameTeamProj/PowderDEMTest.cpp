#include "PowderDEMTest.h"
#include "PowderDropTest.h"
#include "powder/worker.hpp"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "InputCoreTypes.h"
#include "UObject/ConstructorHelpers.h"
#include <algorithm>
#include <stdexcept>

struct FDEMRuntime {powder::Worker Worker;std::shared_ptr<const powder::Snapshot> Latest;uint64 Generation=0;};
void FDEMRuntimeDeleter::operator()(FDEMRuntime* P)const{delete P;}
namespace {
powder::Vec3 V(const FVector& X){return {X.X,X.Y,X.Z};}
std::vector<powder::FixedBox> ReadRig(UWorld* World,powder::TrackingLayout& Layout)
{
 TArray<AStaticMeshActor*> Actors;
 for(TActorIterator<AStaticMeshActor> It(World);It;++It)if(It->ActorHasTag(TEXT("PowderRig")))Actors.Add(*It);
 Actors.Sort([](const AStaticMeshActor& A,const AStaticMeshActor& B){return A.GetPathName()<B.GetPathName();});
 if(Actors.IsEmpty())throw std::invalid_argument("No PowderRig cubes in this level");
 std::vector<powder::FixedBox> Boxes,Feed,Funnel,Tube,Collector;
 for(int32 I=0;I<Actors.Num();++I){
  auto* C=Actors[I]->GetStaticMeshComponent();
  if(!C||!C->GetStaticMesh()||C->GetStaticMesh()->GetPathName()!=TEXT("/Engine/BasicShapes/Cube.Cube"))throw std::invalid_argument("PowderRig requires BasicShapes/Cube");
  if(C->IsSimulatingPhysics())throw std::invalid_argument("Moving Chaos rig walls are unsupported");
  const FTransform T=C->GetComponentTransform();
  const FVector Scale=T.GetScale3D();
  powder::FixedBox B;B.id=I+1;B.weld_group=1;B.centre_m=V(T.GetLocation()*.01);
  B.half_m=V(Scale.GetAbs()*.5);
  B.axes={V(T.GetUnitAxis(EAxis::X)),V(T.GetUnitAxis(EAxis::Y)),V(T.GetUnitAxis(EAxis::Z))};
  // A centred box's sign-reflected local axes describe the same geometric solid.
  Boxes.push_back(B);
  FString Label;
#if WITH_EDITOR
  Label=Actors[I]->GetActorLabel();
#endif
  if(Actors[I]->ActorHasTag(TEXT("PowderTube")))Tube.push_back(B);
  else if(Actors[I]->ActorHasTag(TEXT("PowderFeed"))||Label.StartsWith(TEXT("Feed_Wall_")))Feed.push_back(B);
  else if(Actors[I]->ActorHasTag(TEXT("PowderFunnel"))||Label.StartsWith(TEXT("Feed_Funnel_")))Funnel.push_back(B);
  else if(Actors[I]->ActorHasTag(TEXT("PowderCollector"))||Label.StartsWith(TEXT("Collector_")))Collector.push_back(B);
 }
 powder::validate_boxes(Boxes);Layout=powder::tracking_layout(Feed,Funnel,Tube,Collector);return Boxes;
}
}
APowderDEMTest::APowderDEMTest()
{
 PrimaryActorTick.bCanEverTick=true;
 RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
 ParticleMesh=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DEMParticles"));
 ParticleMesh->SetupAttachment(RootComponent);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 ParticleMesh->SetStaticMesh(Sphere.Object);
 ParticleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 ParticleMesh->SetEnableGravity(false);
 ParticleMesh->SetSimulatePhysics(false);
}
APowderDEMTest::~APowderDEMTest()=default;
void APowderDEMTest::BeginPlay()
{
 Super::BeginPlay();
 Camera=GetWorld()->SpawnActor<ACameraActor>();
 if(Camera){const FVector P=APowderDropTest::OverviewOffset();Camera->SetActorLocation(P);Camera->SetActorRotation((APowderDropTest::OverviewTarget()-P).Rotation());Camera->GetCameraComponent()->SetFieldOfView(55);}
 for(TActorIterator<AStaticMeshActor> It(GetWorld());It;++It)if(It->ActorHasTag(TEXT("PowderCutaway")))It->GetStaticMeshComponent()->SetVisibility(false);
 ResetSimulation();
}
void APowderDEMTest::EndPlay(const EEndPlayReason::Type Reason)
{
 Runtime.Reset();if(Camera)Camera->Destroy();Super::EndPlay(Reason);
}
void APowderDEMTest::ResetSimulation()
{
 Error.Empty();ComputeTime=0;ResetTime=FPlatformTime::Seconds();ParticleMesh->ClearInstances();
 if(!Runtime)Runtime.Reset(new FDEMRuntime());
 try{
  auto Settings=MakeSettings();
  powder::TrackingLayout Layout;powder::ParticleSystem W;W.boxes=ReadRig(GetWorld(),Layout);
  if(MaxResultSamples<2)throw std::invalid_argument("result sample limit must be at least two");
  powder::ResultCriteria Criteria{ResultSampleSeconds,SlowSpeedMps,LongStallSeconds,static_cast<std::size_t>(MaxResultSamples)};powder::validate_criteria(Criteria);
  Runtime->Generation=Runtime->Worker.configure(std::move(Settings),std::move(W),std::move(Layout),Criteria);Runtime->Latest.reset();
 }catch(const std::exception& E){
  // Retire even an invalid replacement, so an old running result cannot reappear.
  Runtime->Generation=Runtime->Worker.reset(powder::ParticleSystem{},-1);Runtime->Latest.reset();
  Error=UTF8_TO_TCHAR(E.what());UE_LOG(LogTemp,Error,TEXT("DEM reset: %s"),*Error);
 }
}
powder::PowderSettings APowderDEMTest::MakeSettings()const{
 if(RandomSeed<0||MaxPlacementCandidates<1)throw std::invalid_argument("seed/budget must be nonnegative/positive");
 if(SizeDistribution!=EDEMSizeDistribution::Mono&&SizeDistribution!=EDEMSizeDistribution::UniformNumber)throw std::invalid_argument("unknown size distribution");
 if(AmountMode!=EDEMAmountMode::Count&&AmountMode!=EDEMAmountMode::Mass)throw std::invalid_argument("unknown amount mode");
 if(ContactLaw!=EDEMContactLaw::Linear&&ContactLaw!=EDEMContactLaw::HertzMindlin)throw std::invalid_argument("unknown contact law");
 if(SupplyMode!=EDEMSupplyMode::Batch&&SupplyMode!=EDEMSupplyMode::Timed)throw std::invalid_argument("unknown supply mode");
 if(ParticlesPerEvent<1)throw std::invalid_argument("supply event count must be positive");
 powder::PowderSettings S;S.supply_mode=SupplyMode==EDEMSupplyMode::Batch?powder::SupplyMode::Batch:powder::SupplyMode::Timed;
 S.supply_start_s=SupplyStartSeconds;S.supply_interval_s=SupplyIntervalSeconds;S.supply_per_event=ParticlesPerEvent;S.retry_interval_s=SupplyRetrySeconds;
 S.seed=RandomSeed;S.max_candidates=MaxPlacementCandidates;
 S.distribution=SizeDistribution==EDEMSizeDistribution::Mono?powder::SizeDistribution::Mono:powder::SizeDistribution::UniformNumber;
 S.diameter_min_m=SizeDistribution==EDEMSizeDistribution::Mono?DiameterM:MinDiameterM;S.diameter_max_m=SizeDistribution==EDEMSizeDistribution::Mono?DiameterM:MaxDiameterM;S.density_kg_m3=DensityKgM3;
 S.amount=AmountMode==EDEMAmountMode::Count?powder::AmountMode::Count:powder::AmountMode::Mass;
 S.count=S.amount==powder::AmountMode::Count?ParticleCount:0;S.target_mass_kg=S.amount==powder::AmountMode::Mass?TargetMassKg:0;S.mass_tolerance_kg=S.amount==powder::AmountMode::Mass?MassToleranceKg:0;
 S.law=ContactLaw==EDEMContactLaw::Linear?powder::ContactLaw::Linear:powder::ContactLaw::HertzMindlin;
 S.particle_material={ParticleYoungPa,ParticlePoisson};S.wall_material={WallYoungPa,WallPoisson};
 S.pair.linear={NormalStiffness,NormalDamping,TangentStiffness,TangentDamping,Friction};S.wall.linear={WallNormalStiffness,WallNormalDamping,WallTangentStiffness,WallTangentDamping,WallFriction};
 S.pair.cn=HertzNormalDamping;S.pair.ct=HertzTangentDamping;S.pair.mu=HertzFriction;S.pair.overlap_cap=HertzOverlapCap;
 S.wall.cn=WallHertzNormalDamping;S.wall.ct=WallHertzTangentDamping;S.wall.mu=WallHertzFriction;S.wall.overlap_cap=WallHertzOverlapCap;
 S.centre_m=V(InitialPositionCm*.01);S.half_m=V(SpawnHalfExtentsCm*.01);S.velocity_m_s=V(InitialVelocityMps);S.gravity_m_s2=V(GravityMps2);S.spacing_factor=SpacingFactor;S.wall_clearance_m=InitialWallClearanceM;S.dt_s=FixedDt;
 powder::validate_settings(S);return S;
}
void APowderDEMTest::ApplySettings(const powder::PowderSettings& S){
 SupplyMode=S.supply_mode==powder::SupplyMode::Batch?EDEMSupplyMode::Batch:EDEMSupplyMode::Timed;
 SupplyStartSeconds=S.supply_start_s;SupplyIntervalSeconds=S.supply_interval_s;ParticlesPerEvent=static_cast<int32>(S.supply_per_event);SupplyRetrySeconds=S.retry_interval_s;
 RandomSeed=S.seed;MaxPlacementCandidates=static_cast<int32>(S.max_candidates);
 SizeDistribution=S.distribution==powder::SizeDistribution::Mono?EDEMSizeDistribution::Mono:EDEMSizeDistribution::UniformNumber;
 DiameterM=S.diameter_min_m;MinDiameterM=S.diameter_min_m;MaxDiameterM=S.diameter_max_m;DensityKgM3=S.density_kg_m3;
 AmountMode=S.amount==powder::AmountMode::Count?EDEMAmountMode::Count:EDEMAmountMode::Mass;
 if(S.amount==powder::AmountMode::Count)ParticleCount=static_cast<int32>(S.count);else{TargetMassKg=S.target_mass_kg;MassToleranceKg=S.mass_tolerance_kg;}
 ContactLaw=S.law==powder::ContactLaw::Linear?EDEMContactLaw::Linear:EDEMContactLaw::HertzMindlin;
 ParticleYoungPa=S.particle_material.young_pa;ParticlePoisson=S.particle_material.poisson;WallYoungPa=S.wall_material.young_pa;WallPoisson=S.wall_material.poisson;
 NormalStiffness=S.pair.linear.kn;NormalDamping=S.pair.linear.cn;TangentStiffness=S.pair.linear.kt;TangentDamping=S.pair.linear.ct;Friction=S.pair.linear.mu;
 WallNormalStiffness=S.wall.linear.kn;WallNormalDamping=S.wall.linear.cn;WallTangentStiffness=S.wall.linear.kt;WallTangentDamping=S.wall.linear.ct;WallFriction=S.wall.linear.mu;
 HertzNormalDamping=S.pair.cn;HertzTangentDamping=S.pair.ct;HertzFriction=S.pair.mu;HertzOverlapCap=S.pair.overlap_cap;
 WallHertzNormalDamping=S.wall.cn;WallHertzTangentDamping=S.wall.ct;WallHertzFriction=S.wall.mu;WallHertzOverlapCap=S.wall.overlap_cap;
 InitialPositionCm=FVector(S.centre_m.x,S.centre_m.y,S.centre_m.z)*100;SpawnHalfExtentsCm=FVector(S.half_m.x,S.half_m.y,S.half_m.z)*100;
 InitialVelocityMps=FVector(S.velocity_m_s.x,S.velocity_m_s.y,S.velocity_m_s.z);GravityMps2=FVector(S.gravity_m_s2.x,S.gravity_m_s2.y,S.gravity_m_s2.z);
 SpacingFactor=S.spacing_factor;InitialWallClearanceM=S.wall_clearance_m;FixedDt=S.dt_s;
}
FString APowderDEMTest::SettingsPath()const{
 if(SettingsFile.IsEmpty()||SettingsFile.Len()>100||FPaths::GetCleanFilename(SettingsFile)!=SettingsFile||!SettingsFile.EndsWith(TEXT(".dem")))throw std::invalid_argument("settings filename must be a simple .dem name");
 for(TCHAR C:SettingsFile)if(!(FChar::IsAlnum(C)||C==TEXT('_')||C==TEXT('-')||C==TEXT('.')))throw std::invalid_argument("settings filename contains invalid characters");
 return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("DEM")/SettingsFile);
}
bool APowderDEMTest::SaveSettings(){
 try{
  const auto Data=powder::serialize_settings(MakeSettings());const FString Path=SettingsPath(),Temp=Path+TEXT(".tmp");
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
  if(!FFileHelper::SaveStringToFile(UTF8_TO_TCHAR(Data.c_str()),*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))throw std::runtime_error("cannot write temporary settings file");
  if(!IFileManager::Get().Move(*Path,*Temp,true,false,false,true))throw std::runtime_error("cannot replace settings file; temporary file retained");
  SettingsStatus=TEXT("Saved: ")+Path;return true;
 }catch(const std::exception& E){SettingsStatus=TEXT("Save failed: ")+FString(UTF8_TO_TCHAR(E.what()));return false;}
}
bool APowderDEMTest::LoadSettings(){
 try{
  const FString Path=SettingsPath();const int64 Size=IFileManager::Get().FileSize(*Path);if(Size<0||Size>65536)throw std::runtime_error("settings file missing or exceeds 64 KiB");
  FString Text;if(!FFileHelper::LoadFileToString(Text,*Path))throw std::runtime_error("cannot read settings file");
  auto Settings=powder::parse_settings(TCHAR_TO_UTF8(*Text));ApplySettings(Settings);ResetSimulation();SettingsStatus=TEXT("Loaded and reset: ")+Path;return true;
 }catch(const std::exception& E){SettingsStatus=TEXT("Load failed (current settings retained): ")+FString(UTF8_TO_TCHAR(E.what()));return false;}
}
FString APowderDEMTest::InitialStatus()const{
 if(!Runtime||!Runtime->Latest||!Runtime->Latest->initial.configured)return TEXT("Initial batch: waiting");
 const auto& I=Runtime->Latest->initial;
 return FString::Printf(TEXT("Seed %llu | d %.6g..%.6g m | target %.6g kg | delta %+.6g kg"),I.seed,I.min_diameter_m,I.max_diameter_m,I.target_mass_kg,I.mass_error_kg);
}
int32 APowderDEMTest::RequestedCount()const{return Runtime&&Runtime->Latest?static_cast<int32>(Runtime->Latest->supply.requested):0;}
int32 APowderDEMTest::PendingCount()const{return Runtime&&Runtime->Latest?static_cast<int32>(Runtime->Latest->supply.pending):0;}
double APowderDEMTest::SupplyMassError()const{return Runtime&&Runtime->Latest?Runtime->Latest->supply.mass_error:0;}
FString APowderDEMTest::SupplyStatus()const{
 if(!Runtime||!Runtime->Latest||!Runtime->Latest->supply.configured)return TEXT("Supply: initializing");
 const auto& S=Runtime->Latest->supply;
 return FString::Printf(TEXT("Supply plan/request/in/wait/future: %llu/%llu/%llu/%llu/%llu\nkg in/wait/future: %.6g / %.6g / %.6g | balance %+.3g kg%s"),uint64(S.planned),uint64(S.requested),uint64(S.inserted),uint64(S.pending),uint64(S.future),S.inserted_mass,S.pending_mass,S.future_mass,S.mass_error,S.pending?TEXT(" | Waiting: occupied/candidate limit"):(S.complete?TEXT(" | Complete"):TEXT(" | Scheduled")));
}
bool APowderDEMTest::ExportResults(){
 FString Dir;
 try{
  if(!Runtime)throw std::runtime_error("no DEM run");const auto S=Runtime->Worker.snapshot();
  if(S->generation!=Runtime->Generation)throw std::runtime_error("wait for current run");
  const auto Files=powder::export_results(*S);
  const FString Id=FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"))+TEXT("_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
  Dir=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("DEM/Results")/Id);
  if(!IFileManager::Get().MakeDirectory(*Dir,true))throw std::runtime_error("cannot create results directory");
  for(const auto& File:Files)if(!FFileHelper::SaveStringToFile(UTF8_TO_TCHAR(File.second.c_str()),*(Dir/UTF8_TO_TCHAR(File.first.c_str())),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))throw std::runtime_error("cannot write result file; incomplete directory retained");
  const FString Run=FString::Printf(TEXT("{\"schema\":1,\"run_id\":\"%s\",\"exported_utc\":\"%s\"}\n"),*Id,*FDateTime::UtcNow().ToIso8601());
  if(!FFileHelper::SaveStringToFile(Run,*(Dir/TEXT("run.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))throw std::runtime_error("cannot write run metadata");
  // Completion marker is written last. Readers must reject directories without it.
  if(!FFileHelper::SaveStringToFile(TEXT("{\"schema\":1,\"complete\":true}\n"),*(Dir/TEXT("complete.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))throw std::runtime_error("cannot publish completion marker");
  LastResultsDirectory=Dir;ExportStatus=TEXT("Exported: ")+Id;return true;
 }catch(const std::exception& E){ExportStatus=TEXT("Export failed: ")+FString(UTF8_TO_TCHAR(E.what()));return false;}
}
FString APowderDEMTest::ResultStatus()const{
 if(!Runtime||!Runtime->Latest)return TEXT("Results: initializing");const auto& S=Runtime->Latest->results;
 return FString::Printf(TEXT("O Export paused run | No outflow %.6g s | Slow/long %llu/%llu%s"),S.current.no_outflow_s,uint64(S.current.slow_count),uint64(S.current.long_slow_count),S.sample_limit_reached?TEXT(" | LIMIT: export then reset"):TEXT(""));
}
void APowderDEMTest::ToggleSimulation(){if(Runtime&&Error.IsEmpty())Runtime->Worker.toggle();}
void APowderDEMTest::PauseSimulation(){if(Runtime)Runtime->Worker.pause();}
void APowderDEMTest::StepSimulation(){if(Runtime&&Error.IsEmpty())Runtime->Worker.single_step();}
void APowderDEMTest::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 if(!bInputReady)if(auto* PC=GetWorld()->GetFirstPlayerController()){
  if(Camera)PC->SetViewTarget(Camera);EnableInput(PC);
  InputComponent->BindKey(EKeys::SpaceBar,IE_Pressed,this,&APowderDEMTest::ToggleSimulation);
  InputComponent->BindKey(EKeys::N,IE_Pressed,this,&APowderDEMTest::StepSimulation);
  InputComponent->BindKey(EKeys::R,IE_Pressed,this,&APowderDEMTest::ResetSimulation);
  InputComponent->BindKey(EKeys::K,IE_Pressed,this,&APowderDEMTest::SaveSettingsKey);
  InputComponent->BindKey(EKeys::L,IE_Pressed,this,&APowderDEMTest::LoadSettingsKey);
  InputComponent->BindKey(EKeys::O,IE_Pressed,this,&APowderDEMTest::ExportResultsKey);
  InputComponent->BindKey(EKeys::T,IE_Pressed,this,&APowderDEMTest::FocusTube);bInputReady=true;
 }
 SyncParticle();
}
void APowderDEMTest::SyncParticle()
{
 if(!Runtime)return;auto Snapshot=Runtime->Worker.snapshot();
 if(Snapshot->generation!=Runtime->Generation||Snapshot==Runtime->Latest)return;
 Runtime->Latest=Snapshot;ComputeTime=Snapshot->compute_seconds;
 if(!Snapshot->ready)return;
 if(!Snapshot->error.empty()){
  const FString NewError=UTF8_TO_TCHAR(Snapshot->error.c_str());if(Error.IsEmpty()){Error=NewError;UE_LOG(LogTemp,Error,TEXT("DEM worker: %s"),*Error);}return;
 }
 const bool Rebuild=ParticleMesh->GetInstanceCount()!=static_cast<int32>(Snapshot->particles.size());
 if(Rebuild)ParticleMesh->ClearInstances();
 for(int32 I=0;I<static_cast<int32>(Snapshot->particles.size());++I){const auto& P=Snapshot->particles[I];
  const FTransform Transform(FQuat(P.orientation.x,P.orientation.y,P.orientation.z,P.orientation.w),FVector(P.position_m.x,P.position_m.y,P.position_m.z)*100,FVector(P.diameter_m));
  if(Rebuild)ParticleMesh->AddInstance(Transform,true);else ParticleMesh->UpdateInstanceTransform(I,Transform,true,false,true);
 }
 ParticleMesh->MarkRenderStateDirty();
}
FVector APowderDEMTest::ParticlePosition()const{FTransform T;if(ParticleMesh->GetInstanceTransform(0,T,true))return T.GetLocation();return InitialPositionCm;}
FVector APowderDEMTest::SolverPositionCm()const{if(!Runtime||!Runtime->Latest||Runtime->Latest->particles.empty())return FVector::ZeroVector;const auto P=Runtime->Latest->particles.front().position_m;return FVector(P.x,P.y,P.z)*100;}
bool APowderDEMTest::TrackingEnabled()const{return Runtime&&Runtime->Latest&&Runtime->Latest->tracking.enabled;}
int32 APowderDEMTest::TrackedCount(int32 Region)const{return Runtime&&Runtime->Latest&&Region>=0&&Region<4?static_cast<int32>(Runtime->Latest->tracking.counts[Region]):0;}
double APowderDEMTest::TrackedMassError()const{return Runtime&&Runtime->Latest?Runtime->Latest->tracking.mass_error_kg:0;}
FString APowderDEMTest::ParticleTracking(int64 Id)const{
 if(Runtime&&Runtime->Latest)for(const auto& R:Runtime->Latest->tracking.records)if(R.id==uint64(Id)){
  const TCHAR* Names[]={TEXT("Feed"),TEXT("Tube"),TEXT("Bin"),TEXT("Outside")};
  return FString::Printf(TEXT("ID %lld %s | entry %.9f s | exit %.9f s | events %llu (-1: none)"),Id,Names[static_cast<unsigned>(R.location)],R.first_entry_s,R.first_exit_s,uint64(R.events.size()));
 }return FString::Printf(TEXT("ID %lld: no current record"),Id);
}
FString APowderDEMTest::TrackingStatus()const{
 if(!Runtime||!Runtime->Latest||!Runtime->Latest->ready)return TEXT("Tracking: initializing");
 const auto& S=Runtime->Latest->tracking;
 if(!S.enabled)return TEXT("Tracking OFF: no named rig groups");
 return FString::Printf(TEXT("Now: Feed %llu | Tube %llu | Bin %llu | Outside %llu | Total %llu"),uint64(S.counts[0]),uint64(S.counts[1]),uint64(S.counts[2]),uint64(S.counts[3]),uint64(S.total_count));
}
FString APowderDEMTest::PassageStatus()const{
 if(!Runtime||!Runtime->Latest)return FString();const auto& S=Runtime->Latest->tracking;
 return FString::Printf(TEXT("Unique in/pass/return %llu/%llu/%llu | Pass events %llu\nMass %.6f kg | balance error %.3g kg | Centre crossings"),uint64(S.unique_entered),uint64(S.unique_passed),uint64(S.unique_returned),S.passages,S.total_mass_kg,S.mass_error_kg);
}
bool APowderDEMTest::IsReady()const{return Runtime&&Runtime->Latest&&Runtime->Latest->ready;}
int32 APowderDEMTest::DisplayedCount()const{return ParticleMesh->GetInstanceCount();}
uint64 APowderDEMTest::RunGeneration()const{return Runtime?Runtime->Generation:0;}
bool APowderDEMTest::IsRunning()const{return Runtime&&Runtime->Latest&&Runtime->Latest->running;}
uint64 APowderDEMTest::StepCount()const{return Runtime&&Runtime->Latest?Runtime->Latest->steps:0;}
double APowderDEMTest::SimulationSeconds()const{return Runtime&&Runtime->Latest?Runtime->Latest->simulation_seconds:0;}
double APowderDEMTest::RealSeconds()const{return FPlatformTime::Seconds()-ResetTime;}
int32 APowderDEMTest::ContactCount()const{return Runtime&&Runtime->Latest?static_cast<int32>(Runtime->Latest->report.wall_contacts+Runtime->Latest->report.pair_contacts):0;}
int32 APowderDEMTest::HistoryCount()const{return Runtime&&Runtime->Latest?static_cast<int32>(Runtime->Latest->histories):0;}
bool APowderDEMTest::ChaosDisabled()const{return !ParticleMesh->IsSimulatingPhysics()&&!ParticleMesh->IsGravityEnabled()&&ParticleMesh->GetCollisionEnabled()==ECollisionEnabled::NoCollision;}
void APowderDEMTest::FocusTube(){if(Camera){const FVector Target(0,0,300);const FVector P=Target+FVector(130,-180,100);Camera->SetActorLocation(P);Camera->SetActorRotation((Target-P).Rotation());}}
void APowderDEMHUD::DrawHUD()
{
 Super::DrawHUD();if(!Canvas)return;
 for(TActorIterator<APowderDEMTest> It(GetWorld());It;++It){auto* T=*It;
  DrawRect(FLinearColor(.015f,.025f,.045f,.9f),16,16,FMath::Min(700.f,Canvas->SizeX-32.f),520);
  DrawText(TEXT("CUSTOM DEM / BACKGROUND WORKER"),FLinearColor(.2f,.8f,1),30,26,nullptr,1.4f);
  DrawText(T->Error.IsEmpty()?(!T->IsReady()?TEXT("INITIALIZING"):T->IsRunning()?TEXT("RUNNING"):TEXT("PAUSED / READY")):TEXT("ERROR - fix settings then R"),FLinearColor::Yellow,30,56);
  DrawText(FString::Printf(TEXT("Simulation: %.9f s | Steps: %llu"),T->SimulationSeconds(),T->StepCount()),FLinearColor::White,30,80);
  DrawText(FString::Printf(TEXT("Wall clock since reset: %.2f s | Compute: %.3f s"),T->RealSeconds(),T->ComputeSeconds()),FLinearColor::White,30,102);
  DrawText(TEXT("SPACE Start/Pause | N One physical step (pauses) | R Reset"),FLinearColor::Yellow,30,126);
  DrawText(TEXT("RMB + WASD/QE: Fly | Mouse: Look | MMB: Pan | Wheel: Speed/Dolly"),FLinearColor::White,30,149);
  DrawText(TEXT("F Focus particle | Home Overview | T Tube | Shift+Esc Stop PIE"),FLinearColor::White,30,172);
  DrawText(FString::Printf(TEXT("Particles: %d | Contacts: %d | Run: %llu | Chaos OFF"),T->DisplayedCount(),T->ContactCount(),T->RunGeneration()),FLinearColor(.4f,1,.6f),30,195);
  DrawText(TEXT("Synthetic particle; uncalibrated. Fixed dt; slower than real time."),FLinearColor(.7f,.8f,.9f),30,217);
  DrawText(TEXT("R Apply settings | K Save | L Load + reset"),FLinearColor(.7f,.8f,.9f),30,239);
  DrawText(T->TrackingStatus(),FLinearColor(.4f,1,.6f),30,266);
  DrawText(T->PassageStatus(),FLinearColor(.4f,1,.6f),30,289);
  DrawText(T->ParticleTracking(T->InspectParticleId),FLinearColor(.4f,1,.6f),30,335);
  DrawText(T->InitialStatus(),FLinearColor(.7f,.8f,.9f),30,360);
  DrawText(T->SettingsStatus,FLinearColor::Yellow,30,383);
  DrawText(T->SupplyStatus(),FLinearColor(.4f,1,.6f),30,407);
  DrawText(T->ResultStatus(),FLinearColor(.7f,.8f,.9f),30,453);
  DrawText(T->ExportStatus,FLinearColor::Yellow,30,478);
  if(!T->Error.IsEmpty()){for(int32 Start=0,Line=0;Start<T->Error.Len();Start+=95,++Line)DrawText(T->Error.Mid(Start,95),FLinearColor::Red,30,540+Line*18);}
  break;
 }
}
