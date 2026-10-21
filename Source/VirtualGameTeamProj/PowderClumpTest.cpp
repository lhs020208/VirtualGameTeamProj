#include "PowderClumpTest.h"
#include "PowderCameraController.h"
#include "powder/clump.hpp"
#include "powder/clump_worker.hpp"
#include <memory>
#include "PowderMeshGeometry.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "InputCoreTypes.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <stdexcept>
struct FClumpRuntime : powder::ClumpRunState {
 FString Configuration;
 std::unique_ptr<powder::ClumpWorker> Worker;
 std::shared_ptr<const powder::ClumpSnapshot> Latest;
};
static powder::ClumpWorkerOptions WorkerOptions(const APowderClumpTest& A){
 powder::ClumpWorkerOptions O;O.paced=A.bRealtimePacing;O.target_rate=A.TargetSimulationRate;O.profile=A.bProfilePerformance;
 O.snapshot_hz=A.DisplaySnapshotHz;O.batch_steps=A.StepsPerFrame;O.batch_budget_ms=A.ComputeBudgetMs;return O;
}
void FClumpDeleter::operator()(FClumpRuntime* P)const{delete P;}
APowderClumpTest::~APowderClumpTest()=default;
void APowderClumpTest::EndPlay(const EEndPlayReason::Type Reason){Runtime.Reset();bRunning=false;Super::EndPlay(Reason);}
APowderClumpTest::APowderClumpTest(){PrimaryActorTick.bCanEverTick=true;auto* Root=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));SetRootComponent(Root);Pebbles=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ClumpPebbles"));Pebbles->SetupAttachment(Root);Pebbles->SetCollisionEnabled(ECollisionEnabled::NoCollision);Pebbles->SetSimulatePhysics(false);GroupBPebbles=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("GroupBPebbles"));GroupBPebbles->SetupAttachment(Root);GroupBPebbles->SetCollisionEnabled(ECollisionEnabled::NoCollision);GroupBPebbles->SetSimulatePhysics(false);Floor=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AnalyticFloor"));Floor->SetupAttachment(Root);Floor->SetCollisionEnabled(ECollisionEnabled::NoCollision);Floor->SetRelativeLocation(FVector(0,0,-5));Floor->SetRelativeScale3D(FVector(1,.8,.1));
 const FVector Locations[]={{-13,0,24},{7,0,14},{25,0,14},{16,-13,14},{16,13,14}};
 const FVector Scales[]={{.36,.32,.02},{.02,.28,.20},{.02,.28,.20},{.16,.02,.20},{.16,.02,.20}};
 for(int32 I=0;I<5;++I){auto* Part=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("MeshRigPart%d"),I));Part->SetupAttachment(Root);Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetRelativeLocation(Locations[I]);Part->SetRelativeScale3D(Scales[I]);if(I==0)Part->SetRelativeRotation(FRotator(-20,0,0));Part->SetVisibility(false);MeshObstacles.Add(Part);}
 Paddle=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrescribedPaddle"));Paddle->SetupAttachment(Root);Paddle->SetCollisionEnabled(ECollisionEnabled::NoCollision);Paddle->SetRelativeLocation(FVector(0,0,4));Paddle->SetRelativeScale3D(FVector(.36,.02,.06));Paddle->SetVisibility(false);
 const FVector TrayLocations[]={{-31,0,10},{31,0,10},{0,-31,10},{0,31,10}};const FVector TrayScales[]={{.02,.64,.20},{.02,.64,.20},{.60,.02,.20},{.60,.02,.20}};
 for(int32 I=0;I<4;++I){auto* Part=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("PaddleTray%d"),I));Part->SetupAttachment(Root);Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetRelativeLocation(TrayLocations[I]);Part->SetRelativeScale3D(TrayScales[I]);Part->SetVisibility(false);TrayWalls.Add(Part);}
}
void APowderClumpTest::OnConstruction(const FTransform& Transform){Super::OnConstruction(Transform);Pebbles->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));Floor->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));ResetSimulation();}
void APowderClumpTest::BeginPlay(){Super::BeginPlay();Pebbles->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));Floor->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));ResetSimulation();if(auto* PC=GetWorld()->GetFirstPlayerController()){Camera=GetWorld()->SpawnActor<ACameraActor>();auto Target=GetActorLocation()+FVector(0,0,8);Camera->SetActorLocation(GetActorLocation()+(Runtime&&Runtime->Mixing?FVector(55,-75,125):FVector(90,-130,95)));Camera->SetActorRotation((Target-Camera->GetActorLocation()).Rotation());PC->SetViewTarget(Camera);EnableInput(PC);InputComponent->BindKey(EKeys::SpaceBar,IE_Pressed,this,&APowderClumpTest::ToggleSimulation);InputComponent->BindKey(EKeys::N,IE_Pressed,this,&APowderClumpTest::StepSimulation);InputComponent->BindKey(EKeys::R,IE_Pressed,this,&APowderClumpTest::ResetSimulation);InputComponent->BindKey(EKeys::O,IE_Pressed,this,&APowderClumpTest::ExportKey);}}
void APowderClumpTest::ResetSimulation(){Runtime.Reset();++SnapshotGeneration;SnapshotSteps=0;TotalPebbles=0;WorkerComputeSeconds=SnapshotCopySeconds=MixingMeasurementTimeSeconds=0;bRunning=false;ProfiledPhysicsSeconds=0;ProfiledDisplaySeconds=0;ProfiledDisplayUpdates=0;ActualSimulationRate=0;SimulationLagSeconds=0;Error.Empty();GeometryDiagnostics.Empty();PhysicalSeconds=0;WallWorkJ=0;WallReactionForceN=WallReactionTorqueNm=FVector::ZeroVector;WallLoadTimeSeconds=0;ExportDirectory.Empty();bMixingIndexValid=false;bMixingFinished=false;MixingIndex=0;ChargeMassKg=0;MixingStatus.Empty();GroupBPebbles->ClearInstances();try{if(!FMath::IsFinite(FixedDt)||FixedDt<=0)throw std::invalid_argument("invalid fixed dt");if(!GetActorRotation().IsNearlyZero()||!GetActorScale3D().Equals(FVector::OneVector))throw std::invalid_argument("clump fixture supports translation only; reset rotation/scale");TUniquePtr<FClumpRuntime,FClumpDeleter> Next(new FClumpRuntime);Next->World=powder::clump_demo(PowderDensity,bMixingRig?GroupBDensity:InclusionDensity);Next->Dt=FixedDt;
 const bool UsePaddle=bPaddleRig||bMixingRig;
 if(UsePaddle&&bTriangleMeshRig)throw std::invalid_argument("select either paddle rig or static mesh rig");
 Paddle->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Paddle->SetRelativeRotation(FQuat::Identity);Paddle->SetRelativeLocation(FVector(0,0,4));Paddle->SetVisibility(UsePaddle);
 for(auto& Part:TrayWalls){Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Part->SetVisibility(UsePaddle);}
 if(UsePaddle){Next->World.boxes.clear();Next->World.meshes.push_back(ExtractDEMStaticMesh(*Floor,GetActorLocation(),1.27,1.30));for(const auto& Part:TrayWalls)Next->World.meshes.push_back(ExtractDEMStaticMesh(*Part,GetActorLocation(),.62,.68));powder::PrescribedMotion Motion;Motion.pivot_m={0,0,.04};Motion.rpm=PaddleRPM;Next->World.moving_meshes.emplace_back(ExtractDEMStaticMesh(*Paddle,GetActorLocation(),.36,.38),Motion);for(auto& Body:Next->World.bodies){Body.position_m.z=.022;Body.orientation={};}
 GeometryDiagnostics=TEXT("Paddle: prescribed constant RPM, 5 static surfaces + 1 moving surface; forces do not change the imposed speed.\n");
 }
 for(auto& Part:MeshObstacles){Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Part->SetVisibility(bTriangleMeshRig);}
 if(bTriangleMeshRig){Next->World.boxes.clear();Next->World.meshes.push_back(ExtractDEMStaticMesh(*Floor,GetActorLocation(),1.27,1.30));
 for(int32 I=0;I<MeshObstacles.Num();++I)Next->World.meshes.push_back(ExtractDEMStaticMesh(*MeshObstacles[I],GetActorLocation(),I==0?.45:.24,I==0?.60:.38));
 for(auto& Body:Next->World.bodies)Body.position_m.z+=.30;
 for(std::size_t I=0;I<Next->World.meshes.size();++I)GeometryDiagnostics+=FString::Printf(TEXT("surface %llu\n%s"),static_cast<uint64>(I),UTF8_TO_TCHAR(Next->World.meshes[I].diagnostics().text().c_str()));
 }
 if(bMixingRig){powder::MixingConfig C;C.count_a=GroupACount;C.count_b=GroupBCount;C.density_a=PowderDensity;C.density_b=GroupBDensity;C.premixed=bStartPremixed;C.dt=FixedDt;C.duration_s=MixingDurationSeconds;C.sample_s=MixingSampleSeconds;C.rpm=PaddleRPM;C.grid={{MixingGrid.X,MixingGrid.Y,MixingGrid.Z}};C.minimum_bodies=MinimumBodiesPerCell;Next->World.bodies=powder::mixing_charge(C);Next->Mixing.emplace(C,Next->World.bodies);}
 GroupBPebbles->SetStaticMesh(Pebbles->GetStaticMesh());if(bMixingRig){if(GroupAMaterial)Pebbles->SetMaterial(0,GroupAMaterial);if(GroupBMaterial)GroupBPebbles->SetMaterial(0,GroupBMaterial);}else Pebbles->SetMaterial(0,nullptr);
 auto Configure=[&](powder::MaterialContact& M,uint32 A,uint32 B,bool Wall){M.law=bHertz?powder::ContactLaw::HertzMindlin:powder::ContactLaw::Linear;M.linear.kn=Wall?(A?InclusionStiffness:PowderStiffness):(A==B?(A?InclusionStiffness:PowderStiffness):MixedStiffness);M.linear.kt=M.linear.kn/4;M.a={A?InclusionYoungPa:PowderYoungPa,.25};M.b={Wall?2e11:(B?InclusionYoungPa:PowderYoungPa),.25};M.cn=M.linear.cn;M.ct=M.linear.ct;M.mu=M.linear.mu;M.rolling.mu=RollingFriction;M.adhesion_gamma_j_m2=Wall?WallAdhesionGamma:PairAdhesionGamma;powder::validate_contact_model(M);};
 for(auto& Entry:Next->World.pair_models)Configure(Entry.second,Entry.first.first,Entry.first.second,false);for(auto& Entry:Next->World.wall_models)Configure(Entry.second,Entry.first,Entry.first,true);
 // Stability validation is performed by the worker before its first snapshot.
 Next->Configuration=FString::Printf(TEXT("clump_fixture_v1\ncore %s\ndt %.17g\npowder_density %.17g\ninclusion_density %.17g\npowder_kn %.17g\nmixed_kn %.17g\ninclusion_kn %.17g\nhertz %d\npowder_E %.17g\ninclusion_E %.17g\nrolling_mu %.17g\npair_gamma %.17g\nwall_gamma %.17g\n"),UTF8_TO_TCHAR(powder::Version),FixedDt,PowderDensity,bMixingRig?GroupBDensity:InclusionDensity,PowderStiffness,MixedStiffness,InclusionStiffness,bHertz?1:0,PowderYoungPa,InclusionYoungPa,RollingFriction,PairAdhesionGamma,WallAdhesionGamma);
 Next->Configuration+=FString::Printf(TEXT("triangle_mesh_rig %d\n"),bTriangleMeshRig?1:0);
 Next->Configuration+=FString::Printf(TEXT("paddle_rig %d\npaddle_applied_rpm %.17g\npaddle_pivot_m 0 0 0.04\npaddle_axis 0 0 1\npaddle_linear_velocity_m_s 0 0 0\npaddle_phase_rad 0\n"),UsePaddle?1:0,UsePaddle?PaddleRPM:0);
 Next->Worker=std::make_unique<powder::ClumpWorker>(static_cast<const powder::ClumpRunState&>(*Next),WorkerOptions(*this));Runtime=MoveTemp(Next);Sync();}catch(const std::exception& E){Runtime.Reset();Pebbles->ClearInstances();GroupBPebbles->ClearInstances();Error=UTF8_TO_TCHAR(E.what());}}
void APowderClumpTest::ToggleSimulation(){
 if(!Runtime||!Error.IsEmpty())return;
 try{if(bRunning)Runtime->Worker->pause();else{Runtime->Worker->set_options(WorkerOptions(*this));Runtime->Worker->run();}Sync();}
 catch(const std::exception& E){Error=UTF8_TO_TCHAR(E.what());bRunning=false;}
}
void APowderClumpTest::StepSimulation(){if(!Runtime||!Error.IsEmpty())return;Runtime->Worker->single_step();Sync();}
void APowderClumpTest::Tick(float DeltaSeconds){Super::Tick(DeltaSeconds);if(GEngine&&GetWorld()->GetFirstPlayerController()&&Runtime&&Runtime->Mixing)GEngine->AddOnScreenDebugMessage(2121,0,FColor::Green,FString::Printf(TEXT("Mixing: mass-weighted cell uniformity %s (not product quality)\n%s"),bMixingIndexValid?*FString::Printf(TEXT("%.6f"),MixingIndex):TEXT("N/A"),*MixingStatus));if(GEngine&&GetWorld()->GetFirstPlayerController()&&Runtime&&!Runtime->World.moving_meshes.empty())GEngine->AddOnScreenDebugMessage(2020,0,FColor::Yellow,FString::Printf(TEXT("Applied RPM %.2f | wall work %.6g J | last reaction Tz %.6g Nm"),AppliedPaddleRPM(),WallWorkJ,WallReactionTorqueNm.Z));Sync();
if(GEngine&&GetWorld()->GetFirstPlayerController())GEngine->AddOnScreenDebugMessage(2626,0,FColor::Cyan,FString::Printf(TEXT("Speed target %.2fx | actual %.4fx | lag %.3f s | worker batch %.0f ms | %s"),TargetSimulationRate,ActualSimulationRate,SimulationLagSeconds,ComputeBudgetMs,bRealtimePacing?TEXT("fixed-step real-time target"):TEXT("unpaced worker")));
if(GEngine&&GetWorld()->GetFirstPlayerController())GEngine->AddOnScreenDebugMessage(1818,0,FColor::Cyan,FString::Printf(TEXT("%s | %d bodies / %d shown of %d pebbles | %d triangles | t=%.6f s | Space run/pause, N step, R reset, O export\n%s"),Runtime&&!Runtime->World.moving_meshes.empty()?TEXT("Paddle test"):Runtime&&!Runtime->World.meshes.empty()?TEXT("Mesh test"):TEXT("Clump test"),BodyCount(),RenderedPebbles(),TotalPebbles,MeshTriangleCount(),PhysicalSeconds,*Error));}
void APowderClumpTest::Sync(){
 if(!Runtime||!Runtime->Worker)return;
 auto S=Runtime->Worker->snapshot();if(!S||S==Runtime->Latest)return;
 if(Runtime->Latest&&(S->generation<Runtime->Latest->generation||(S->generation==Runtime->Latest->generation&&(S->sequence<=Runtime->Latest->sequence||S->steps<Runtime->Latest->steps))))return;
 const double Start=bProfilePerformance?FPlatformTime::Seconds():0;
 Runtime->Latest=S;Runtime->World.bodies=S->bodies;Runtime->World.physical_time_s=S->time_s;Runtime->World.moving_wall_work_j=S->wall_work_j;
 Runtime->Steps=S->steps;Runtime->Profile=S->profile;SnapshotSteps=static_cast<int64>(S->steps);bRunning=S->running;Error=UTF8_TO_TCHAR(S->error.c_str());
 PhysicalSeconds=S->time_s;WallLoadTimeSeconds=S->load_time_s;WallWorkJ=S->wall_work_j;ActualSimulationRate=S->actual_rate;SimulationLagSeconds=S->lag_s;
 WorkerComputeSeconds=S->compute_s;SnapshotCopySeconds=S->snapshot_s;ProfiledPhysicsSeconds=S->profile.total_seconds();
 if(!S->report.moving_wall_loads.empty()){const auto& L=S->report.moving_wall_loads[0];WallReactionForceN=FVector(L.reaction_force_n.x,L.reaction_force_n.y,L.reaction_force_n.z);WallReactionTorqueNm=FVector(L.reaction_torque_nm.x,L.reaction_torque_nm.y,L.reaction_torque_nm.z);}
 if(S->mixing){const auto& M=*S->mixing;bMixingIndexValid=M.valid;MixingIndex=M.index;bMixingFinished=S->finished;ChargeMassKg=M.mass[0]+M.mass[1];MixingMeasurementTimeSeconds=M.time_s;
  MixingStatus=FString::Printf(TEXT("A blue %d / B orange %d | %.6g kg | occupied %d | sparse %d | outside %d | %s%s | sampled t=%.6f s"),M.count[0],M.count[1],ChargeMassKg,M.occupied,M.undersampled,M.outside,UTF8_TO_TCHAR(M.status.c_str()),S->finished?TEXT(" | FINISHED"):TEXT(""),M.time_s);
 }
 if(!Runtime->World.moving_meshes.empty()){const auto& M=Runtime->World.moving_meshes[0].motion();auto Q=M.rotation(S->time_s);auto P=M.point({0,0,.04},S->time_s);Paddle->SetRelativeLocation(FVector(P.x,P.y,P.z)*100);Paddle->SetRelativeRotation(FQuat(Q.x,Q.y,Q.z,Q.w));}
 TotalPebbles=0;for(const auto& B:S->bodies)TotalPebbles+=static_cast<int32>(B.pebbles.size());
 TArray<FTransform> ATransforms,BTransforms;
 const int32 Stride=MaxDisplayedPebbles>0?FMath::Max(1,FMath::DivideAndRoundUp(TotalPebbles,MaxDisplayedPebbles)):TotalPebbles+1;
 int32 Index=0;
 if(MaxDisplayedPebbles>0)for(const auto& B:S->bodies){
  // Select stable body/local-pebble indices; display LOD never enters the solver.
  for(const auto& P:powder::clump_particles(B)){if(MaxDisplayedPebbles>0&&Index%Stride==0){FTransform T(FQuat(P.orientation.x,P.orientation.y,P.orientation.z,P.orientation.w),FVector(P.position_m.x,P.position_m.y,P.position_m.z)*100,FVector(P.diameter_m));(Runtime->Mixing&&Runtime->Mixing->groups.at(B.id)==1?BTransforms:ATransforms).Add(T);}++Index;}
 }
 auto Apply=[](UInstancedStaticMeshComponent* C,const TArray<FTransform>& T){
  if(C->GetInstanceCount()!=T.Num()){C->ClearInstances();if(!T.IsEmpty())C->AddInstances(T,false,false,false);}
  else if(!T.IsEmpty())C->BatchUpdateInstancesTransforms(0,T,false,true,true);
 };
 Apply(Pebbles,ATransforms);Apply(GroupBPebbles,BTransforms);
 if(bProfilePerformance){ProfiledDisplaySeconds+=FPlatformTime::Seconds()-Start;++ProfiledDisplayUpdates;}
}
int32 APowderClumpTest::RenderedPebbles()const{return Pebbles->GetInstanceCount()+GroupBPebbles->GetInstanceCount();}
int32 APowderClumpTest::RenderedGroupB()const{return GroupBPebbles->GetInstanceCount();}
int32 APowderClumpTest::MixingSampleCount()const{return Runtime&&Runtime->Latest?static_cast<int32>(Runtime->Latest->sample_count):0;}
int32 APowderClumpTest::BodyCount()const{return Runtime?static_cast<int32>(Runtime->World.bodies.size()):0;}
FVector APowderClumpTest::BodyPositionCm(int32 Index)const{if(!Runtime||Index<0||Index>=BodyCount())return {};auto P=Runtime->World.bodies[Index].position_m;return FVector(P.x,P.y,P.z)*100;}
FQuat APowderClumpTest::DisplayedPaddleRotation()const{return Paddle->GetRelativeRotation().Quaternion();}
double APowderClumpTest::AppliedPaddleRPM()const{return Runtime&&!Runtime->World.moving_meshes.empty()?Runtime->World.moving_meshes[0].motion().rpm:0;}
int32 APowderClumpTest::MeshTriangleCount()const{int32 Count=0;if(Runtime)for(const auto& M:Runtime->World.meshes)Count+=static_cast<int32>(M.diagnostics().triangles);if(Runtime)for(const auto& M:Runtime->World.moving_meshes)Count+=static_cast<int32>(M.reference().diagnostics().triangles);return Count;}
bool APowderClumpTest::ExportState(){if(!Runtime||bRunning||!Error.IsEmpty())return false;try{static_cast<powder::ClumpRunState&>(*Runtime)=Runtime->Worker->checkpoint();}catch(const std::exception& E){Error=UTF8_TO_TCHAR(E.what());return false;}const FString Dir=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("DEM/Clumps")/FGuid::NewGuid().ToString());IFileManager::Get().MakeDirectory(*Dir,true);auto Save=[&](const TCHAR* Name,const FString& Text){return FFileHelper::SaveStringToFile(Text,*(Dir/Name),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);};for(std::size_t I=0;I<Runtime->World.meshes.size();++I){auto Name=FString::Printf(TEXT("surface_%llu.mesh"),static_cast<uint64>(I));if(!Save(*Name,UTF8_TO_TCHAR(Runtime->World.meshes[I].serialize().c_str())))return false;}if(!Runtime->World.meshes.empty()&&!Save(TEXT("geometry_diagnostics.txt"),GeometryDiagnostics))return false;for(std::size_t I=0;I<Runtime->World.moving_meshes.size();++I){auto Name=FString::Printf(TEXT("moving_reference_%llu.mesh"),static_cast<uint64>(I));if(!Save(*Name,UTF8_TO_TCHAR(Runtime->World.moving_meshes[I].reference().serialize().c_str())))return false;}
 if(!Runtime->World.moving_meshes.empty()){auto Q=Runtime->World.moving_meshes[0].motion().rotation(PhysicalSeconds);FString MotionState=FString::Printf(TEXT("state_time_s,last_load_time_s,rpm,qw,qx,qy,qz,wall_work_to_particles_J,reaction_Fx_N,reaction_Fy_N,reaction_Fz_N,reaction_Tx_Nm,reaction_Ty_Nm,reaction_Tz_Nm\n%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g\n"),PhysicalSeconds,WallLoadTimeSeconds,AppliedPaddleRPM(),Q.w,Q.x,Q.y,Q.z,WallWorkJ,WallReactionForceN.X,WallReactionForceN.Y,WallReactionForceN.Z,WallReactionTorqueNm.X,WallReactionTorqueNm.Y,WallReactionTorqueNm.Z);if(!Save(TEXT("paddle_state.csv"),MotionState))return false;}
 if(Runtime->Mixing){try{for(const auto& Entry:Runtime->Mixing->files(Runtime->World.bodies,Runtime->Steps))if(!Save(UTF8_TO_TCHAR(Entry.first.c_str()),UTF8_TO_TCHAR(Entry.second.c_str())))return false;}catch(const std::exception& E){Error=UTF8_TO_TCHAR(E.what());return false;}}
 if(Runtime->Profile.attempted_steps||ProfiledDisplayUpdates){std::ostringstream P;P<<std::setprecision(17)<<"phase,cpu_seconds,attempted_steps,accepted_steps,pair_checks,post_pair_checks,material_pair_validations\n";for(std::size_t I=0;I<Runtime->Profile.seconds.size();++I)P<<powder::ClumpProfile::names[I]<<','<<Runtime->Profile.seconds[I]<<','<<Runtime->Profile.attempted_steps<<','<<Runtime->Profile.accepted_steps<<','<<Runtime->Profile.pair_distance_checks<<','<<Runtime->Profile.post_pair_checks<<','<<Runtime->Profile.material_pair_validations<<'\n';P<<"worker_compute_cpu,"<<WorkerComputeSeconds<<",0,0,0,0,0\n";P<<"snapshot_copy_cpu,"<<SnapshotCopySeconds<<",0,0,0,0,0\n";P<<"display_sync_cpu,"<<ProfiledDisplaySeconds<<",0,0,0,0,0\n";if(!Save(TEXT("performance.csv"),UTF8_TO_TCHAR(P.str().c_str())))return false;}
 std::ostringstream Nodes;Nodes<<std::setprecision(17)<<"body_id,type,local_x_m,local_y_m,local_z_m,diameter_m\n";for(auto& B:Runtime->World.bodies)for(auto& P:B.pebbles)Nodes<<B.id<<','<<B.type<<','<<P.local_m.x<<','<<P.local_m.y<<','<<P.local_m.z<<','<<P.diameter_m<<'\n';if(!Save(TEXT("bodies.csv"),UTF8_TO_TCHAR(powder::serialize_clump_state(Runtime->World).c_str()))||!Save(TEXT("pebbles.csv"),UTF8_TO_TCHAR(Nodes.str().c_str()))||!Save(TEXT("configuration.txt"),Runtime->Configuration+FString::Printf(TEXT("steps %llu\nphysical_s %.17g\n"),Runtime->Steps,PhysicalSeconds))||!Save(TEXT("complete.json"),TEXT("{\"complete\":true,\"schema\":1}")))return false;ExportDirectory=Dir;return true;}
APowderClumpGameMode::APowderClumpGameMode(){DefaultPawnClass=nullptr;PlayerControllerClass=APowderCameraController::StaticClass();}
void APowderClumpGameMode::StartPlay(){Super::StartPlay();for(TActorIterator<APowderClumpTest> It(GetWorld());It;++It)return;GetWorld()->SpawnActor<APowderClumpTest>();}
