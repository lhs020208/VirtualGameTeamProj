#include "PowderGranularMixer.h"
#include "powder/granular.hpp"
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

struct FGranularRuntime {
 std::atomic<bool> Quit{false},Run{true},Gate{false},Auto{true};std::atomic<double> Rpm{30},Rate{.1},MixEnd{9};
 std::thread Worker;std::mutex Lock;std::vector<powder::Grain> Positions;std::string Error;double Time=0,Wall=0,Mix=0,Angle=0,Radius=0;int Receiver=0;uint64 Generation=0;
 FGranularRuntime(powder::GranularConfig C,const FString& Folder,int Batch){std::filesystem::path Dir(*Folder);
 Worker=std::thread([this,C,Dir,Batch]{try{
  powder::GranularGPU Solver(C);Radius=Solver.radius();std::ofstream Csv(Dir/"trace.csv");if(!Csv)throw std::runtime_error("cannot open trace");Csv<<"time_s,active_wall_s,rate,mix_index,receiver_count,total_count,mass_kg,gate\n";
  auto Last=std::chrono::steady_clock::now(),Start=Last,LastDisplay=Last;double Debt=0,Active=0,NextRecord=0,Theta=0;
  while(!Quit){auto Now=std::chrono::steady_clock::now();double Delta=std::chrono::duration<double>(Now-Last).count();Last=Now;
   if(!Run){std::this_thread::sleep_for(std::chrono::milliseconds(3));continue;}
   Active+=Delta;Debt+=Delta*Rate.load();unsigned Steps=static_cast<unsigned>(std::min(double(Batch),std::floor(Debt/C.dt)));
   if(!Steps){std::this_thread::sleep_for(std::chrono::milliseconds(1));continue;}
   double T=Solver.time();bool Open=!C.test_mixer&&(Auto?(T>=MixEnd):Gate.load());float Speed=static_cast<float>((Auto&&T<1)?0:Rpm.load());Solver.advance(Steps,Speed,Open);Debt=FMath::Max(0.,Debt-double(Steps)*C.dt);Theta=Solver.angle();
   Now=std::chrono::steady_clock::now();if(std::chrono::duration<double>(Now-LastDisplay).count()<.05)continue;LastDisplay=Now;
   auto Data=Solver.snapshot();std::array<std::array<unsigned,2>,64> Bins{};unsigned A=0,Received=0,InMixer=0;for(const auto& P:Data){int X=FMath::Clamp(int((P.x+(C.test_mixer?.75f:.45f))/(C.test_mixer?1.5f:.9f)*4),0,3),Y=FMath::Clamp(int((P.y+(C.test_mixer?.45f:.25f))/(C.test_mixer?.9f:.5f)*4),0,3),Z=FMath::Clamp(int((P.z+(C.test_mixer?.325f:0))/(C.test_mixer?.65f:.4f)*4),0,3);if(C.test_mixer||P.z>=0){++InMixer;Bins[(Z*4+Y)*4+X][P.w==0?0:1]++;if(P.w==0)++A;}if(!C.test_mixer&&P.z<-.25f)++Received;}
   double F=double(A)/FMath::Max(1u,InMixer),Variance=0;for(auto B:Bins){double N=B[0]+B[1];if(N)Variance+=N/FMath::Max(1u,InMixer)*std::pow(B[0]/N-F,2);}double I=(InMixer>1&&A>0&&A<InMixer)?1-Variance/FMath::Max(1.e-9,F*(1-F)):-1;
   T=Solver.time();if(T>=NextRecord){Csv<<T<<','<<Active<<','<<T/Active<<','<<I<<','<<Received<<','<<Data.size()<<",64.8,"<<Open<<'\n';Csv.flush();if(!Csv)throw std::runtime_error("trace write failed");std::ofstream Bin(Dir/"final.bin",std::ios::binary);Bin.write(reinterpret_cast<const char*>(Data.data()),Data.size()*sizeof(powder::Grain));if(!Bin)throw std::runtime_error("snapshot write failed");NextRecord=T+.25;}
   {std::lock_guard<std::mutex> Guard(Lock);Positions=std::move(Data);Time=T;Wall=Active;Mix=I;Angle=Theta;Receiver=static_cast<int>(Received);++Generation;}
  }
 }catch(const std::exception& E){std::lock_guard<std::mutex> Guard(Lock);Error=E.what();}});
 }
 ~FGranularRuntime(){Quit=true;if(Worker.joinable())Worker.join();}
};
APowderGranularMixer::APowderGranularMixer(){PrimaryActorTick.bCanEverTick=true;auto* Root=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));RootComponent=Root;
 Shaft=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shaft"));Shaft->SetupAttachment(Root);Shaft->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));Shaft->SetRelativeLocation(FVector(0,0,21));Shaft->SetRelativeRotation(FRotator(0,0,90));Shaft->SetRelativeRotation(FQuat(FVector::YAxisVector,PI/2));Shaft->SetRelativeScale3D(FVector(.05,.05,.9));Shaft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 GateMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OutletGate"));GateMesh->SetupAttachment(Root);GateMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));GateMesh->SetRelativeLocation(FVector(0,0,-1));GateMesh->SetRelativeScale3D(FVector(.12,.12,.02));GateMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 GroupA=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RedGrains"));GroupB=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BlueGrains"));Walls=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CutawayVessel"));Blades=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RotatingPaddles"));
 for(auto* C:{GroupA.Get(),GroupB.Get(),Walls.Get(),Blades.Get()}){C->SetupAttachment(Root);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetMobility(EComponentMobility::Movable);C->SetCastShadow(false);C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,C==GroupA||C==GroupB?TEXT("/Engine/BasicShapes/Sphere.Sphere"):TEXT("/Engine/BasicShapes/Cube.Cube")));}}
APowderGranularMixer::~APowderGranularMixer(){Stop();}
void APowderGranularMixer::Stop(){delete Runtime;Runtime=nullptr;}
void APowderGranularMixer::Geometry(){
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
void APowderGranularMixer::ResetSimulation(){Stop();GroupA->ClearInstances();GroupB->ClearInstances();Generation=0;Error.Empty();PhysicalSeconds=0;FinishWall=0;Shot1=false;Shot2=false;if(!HasActorBegunPlay())return;
 OutputDir=FPaths::ProjectSavedDir()/(TEXT("Granular_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))+TEXT("_")+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));FString Override;if(FParse::Value(FCommandLine::Get(),TEXT("GrainOutput="),Override))OutputDir=Override;IFileManager::Get().MakeDirectory(*OutputDir,true);
 powder::GranularConfig C;C.count=ParticleCount;C.iterations=ContactIterations;C.dt=static_cast<float>(TimeStep);C.test_mixer=TestMixerAsset;C.friction=static_cast<float>(Friction);Runtime=new FGranularRuntime(C,OutputDir,FMath::Clamp(SolverBatchSize,1,128));Runtime->Run=Running;Runtime->Rate=TargetRate;Runtime->Rpm=RPM;Runtime->Auto=AutomaticSequence;Runtime->MixEnd=MixUntilSeconds;}
void APowderGranularMixer::BeginPlay(){Super::BeginPlay();Geometry();Benchmark=FParse::Param(FCommandLine::Get(),TEXT("GrainBenchmark"));if(Benchmark){FParse::Value(FCommandLine::Get(),TEXT("GrainCount="),ParticleCount);TargetRate=2;FParse::Value(FCommandLine::Get(),TEXT("GrainDt="),TimeStep);FParse::Value(FCommandLine::Get(),TEXT("GrainIterations="),ContactIterations);FParse::Value(FCommandLine::Get(),TEXT("GrainBatch="),SolverBatchSize);FParse::Value(FCommandLine::Get(),TEXT("GrainSeconds="),BenchmarkSeconds);if(FParse::Param(FCommandLine::Get(),TEXT("GrainTestMixer")))TestMixerAsset=true;}Geometry();ResetSimulation();auto* Camera=GetWorld()->SpawnActor<ACameraActor>();Camera->SetActorLocation(GetActorTransform().TransformPosition((TestMixerAsset?FVector(170,-210,115):FVector(115,-160,95))));Camera->SetActorRotation((GetActorTransform().TransformPosition(FVector(0,0,-5))-Camera->GetActorLocation()).Rotation());if(auto* PC=UGameplayStatics::GetPlayerController(this,0))PC->SetViewTarget(Camera);}
void APowderGranularMixer::EndPlay(const EEndPlayReason::Type Reason){Stop();Super::EndPlay(Reason);}
void APowderGranularMixer::Display(){if(!Runtime)return;std::unique_lock<std::mutex> Guard(Runtime->Lock);Error=UTF8_TO_TCHAR(Runtime->Error.c_str());if(Generation==Runtime->Generation)return;Generation=Runtime->Generation;PhysicalSeconds=Runtime->Time;ActualRate=Runtime->Wall>0?PhysicalSeconds/Runtime->Wall:0;RadiusMm=Runtime->Radius*1000;MixingIndex=Runtime->Mix;ReceiverCount=Runtime->Receiver;ReceiverMassKg=64.8*ReceiverCount/ParticleCount;auto Points=Runtime->Positions;double DisplayAngle=Runtime->Angle,DisplayRadius=Runtime->Radius;Guard.unlock();
 TArray<FTransform> Data[2];for(const auto& P:Points)Data[P.w==0?0:1].Add(FTransform(FQuat::Identity,FVector(P.x,P.y,P.z)*100,FVector(DisplayRadius*2)));UInstancedStaticMeshComponent* Groups[]={GroupA,GroupB};for(int K=0;K<2;K++){if(Groups[K]->GetInstanceCount()!=Data[K].Num()){Groups[K]->ClearInstances();Groups[K]->AddInstances(Data[K],false,false);}else Groups[K]->BatchUpdateInstancesTransforms(0,Data[K],false,true,true);}
 if(TestMixerAsset){Blades->UpdateInstanceTransform(0,FTransform(FQuat(FVector::ForwardVector,DisplayAngle)),false,true,true);return;}
 for(int K=0;K<6;K++){double A=DisplayAngle+K*PI/3;FQuat Q(FVector::ForwardVector,A);Blades->UpdateInstanceTransform(K,FTransform(Q*FQuat(FVector::UpVector,K%2?-.4:.4),FVector(-37.5+K*15,0,21)+Q.RotateVector(FVector(0,9.5,0)),FVector(.09,.14,.024)),false,K==5,true);}
}
void APowderGranularMixer::Tick(float Delta){Super::Tick(Delta);if(!Runtime)return;Runtime->Run=Running;Runtime->Rate=FMath::Clamp(TargetRate,.01,2.);Runtime->Rpm=FMath::Clamp(RPM,0.,60.);Runtime->Gate=GateOpen;Runtime->Auto=AutomaticSequence;Runtime->MixEnd=MixUntilSeconds;Display();GateMesh->SetVisibility(!TestMixerAsset&&!(AutomaticSequence?PhysicalSeconds>=MixUntilSeconds:GateOpen));Phase=AutomaticSequence?(PhysicalSeconds<1?TEXT("settle"):PhysicalSeconds<MixUntilSeconds?TEXT("mix"):TEXT("discharge")):(GateOpen?TEXT("gate open"):TEXT("gate closed"));
 if(TestMixerAsset)Phase=TEXT("MX301 TEST ONLY | closed mixing | 64.8kg");
 if(GEngine)GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this),0,FColor::Cyan,FString::Printf(TEXT("GPU granular APPROX | %s | N %d r %.2fmm | t %.2fs %.3fx | I %.3f receiver %d | %s"),*Phase,ParticleCount,RadiusMm,PhysicalSeconds,ActualRate,MixingIndex,ReceiverCount,*Error));
 if(Benchmark){if(PhysicalSeconds>3&&!Shot1){FScreenshotRequest::RequestScreenshot(OutputDir/TEXT("mix.png"),true,false);Shot1=true;}if(PhysicalSeconds>10&&!Shot2){FScreenshotRequest::RequestScreenshot(OutputDir/TEXT("discharge.png"),true,false);Shot2=true;}if((PhysicalSeconds>=BenchmarkSeconds||!Error.IsEmpty())&&FinishWall==0){Running=false;FinishWall=FPlatformTime::Seconds();FString J=FString::Printf(TEXT("{\"count\":%d,\"time\":%.9f,\"rate\":%.9f,\"receiver\":%d,\"error\":\"%s\",\"physical_accuracy_validated\":false}"),ParticleCount,PhysicalSeconds,ActualRate,ReceiverCount,*Error);FFileHelper::SaveStringToFile(J,*(OutputDir/TEXT("result.json")));}if(FinishWall>0&&FPlatformTime::Seconds()-FinishWall>1)FPlatformMisc::RequestExit(false);}
}

