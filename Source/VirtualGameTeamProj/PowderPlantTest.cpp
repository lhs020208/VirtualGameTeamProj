#include "PowderPlantTest.h"
#include "PlantVisibilityController.h"
#include "Camera/CameraActor.h"
#include "powder/plant.hpp"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "HAL/IConsoleManager.h"
#include "UnrealClient.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <sstream>
#include <chrono>
namespace {
std::vector<powder::Grain> ReadCharge(const FString& Path){
 FString Text;if(!FFileHelper::LoadFileToString(Text,*Path))throw std::runtime_error("particle charge file missing");
 std::istringstream File(TCHAR_TO_UTF8(*Text));std::string Line;std::getline(File,Line);std::vector<powder::Grain> P;
 while(std::getline(File,Line)){std::replace(Line.begin(),Line.end(),',',' ');std::istringstream Row(Line);unsigned Id,Type;std::string Supplier;float X,Y,Z,R,D;
  if(!(Row>>Id>>Type>>Supplier>>X>>Y>>Z>>R>>D)||Id!=P.size()||std::abs(R-.00125f)>1e-8||D!=1200||Type>=32)throw std::runtime_error("invalid virtual charge; plant test requires radius 1.25mm/density 1200");P.push_back({X,Y,Z,float(Type)});
 }if(P.empty())throw std::runtime_error("empty particle charge");return P;
}
void Count(const std::vector<powder::Grain>&P,int&WH,int&RH,int&MX){WH=RH=MX=0;for(auto A:P){if(std::hypot(A.x,A.y)<.355&&A.z>.603&&A.z<1.78)++WH;if(std::hypot(A.x-3,A.y)<.255&&A.z>2.163&&A.z<3.7)++RH;if(A.x>2.25&&A.x<3.75&&std::abs(A.y)<.45&&A.z>.57&&A.z<1.31)++MX;}}
}
struct FPlantRuntime {
 std::thread Thread;std::mutex Mutex;std::atomic<bool> Quit{false},Run{true};std::atomic<double> Rate{.1},Drop{1000},RPM{15};
 std::vector<powder::Grain> Points;std::string Error;double Time=0,Wall=0,Angle=0;int Phase=0,WH=0,RH=0,MX=0;uint64 Generation=1;
 FPlantRuntime(powder::GranularConfig C,std::vector<powder::Grain> P,std::vector<powder::PlantRoute> Routes,double Timeout,double Fraction,double TransferWait,bool Start,double RequestedRate,double Pressure,double Speed):Points(P){Run=Start;Rate=RequestedRate;Drop=Pressure;RPM=Speed;
  Thread=std::thread([this,C=std::move(C),P=std::move(P),Routes=std::move(Routes),Timeout,Fraction,TransferWait]()mutable{
   try{double Applied=Drop;powder::solve_plant_pressure(C.plant_data,Routes,Applied);powder::GranularGPU GPU(C,P,std::vector<powder::Grain>(P.size()));int PhaseNow=0;double StageStart=0,Active=0;auto LastPublish=std::chrono::steady_clock::now();
    while(!Quit){if(!Run){std::this_thread::sleep_for(std::chrono::milliseconds(5));continue;}auto Begin=std::chrono::steady_clock::now();
     double R=Rate,S=RPM,D=Drop;if(!std::isfinite(R)||R<=0||R>.1||!std::isfinite(S)||S<0||S>30)throw std::runtime_error("invalid live rate/RPM");
     if(D!=Applied){powder::solve_plant_pressure(C.plant_data,Routes,D);GPU.update_plant_field(C.plant_data);Applied=D;}
     GPU.advance(16,float(S),false);double Compute=std::chrono::duration<double>(std::chrono::steady_clock::now()-Begin).count();double Sleep=16*C.dt/R-Compute;
     if(Sleep>0)std::this_thread::sleep_for(std::chrono::duration<double>(Sleep));Active+=std::chrono::duration<double>(std::chrono::steady_clock::now()-Begin).count();
     if(std::chrono::duration<double>(std::chrono::steady_clock::now()-LastPublish).count()>.1){auto Next=GPU.snapshot();int W,H,M;Count(Next,W,H,M);
      bool Advance=(PhaseNow==0&&W==int(Next.size()))||(PhaseNow==1&&powder::plant_discharge_ready(H,int(Next.size()),GPU.time()-StageStart,Fraction,TransferWait));
      if(Advance){++PhaseNow;StageStart=GPU.time();C.plant_data[4].x=float(PhaseNow);GPU.update_plant_field(C.plant_data);}
      {std::lock_guard<std::mutex>L(Mutex);Points=std::move(Next);Time=GPU.time();Angle=GPU.angle();Wall=Active;WH=W;RH=H;MX=M;Phase=PhaseNow;++Generation;}
      if(GPU.time()-StageStart>Timeout&&((PhaseNow<2)||(M<int(P.size()))))throw std::runtime_error("stage timeout: retained particles require flow/geometry review; no forced transfer");LastPublish=std::chrono::steady_clock::now();
     }
    }
   }catch(const std::exception&E){std::lock_guard<std::mutex>L(Mutex);Error=E.what();Run=false;}
  });
 }
 ~FPlantRuntime(){Quit=true;if(Thread.joinable())Thread.join();}
};
APowderPlantTest::APowderPlantTest(){PrimaryActorTick.bCanEverTick=true;RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));}
APowderPlantTest::~APowderPlantTest(){Stop();}
void APowderPlantTest::Stop(){delete Runtime;Runtime=nullptr;}
void APowderPlantTest::CacheRotor(){Rotor.Empty();for(TActorIterator<AActor>It(GetWorld());It;++It){const FString N=It->GetActorNameOrLabel();if(N.StartsWith(TEXT("MX-301_"))&&(N.Contains(TEXT("Paddle"))||N.Contains(TEXT("Shaft")))&&!N.Contains(TEXT("Drive"))&&!N.Contains(TEXT("Support"))){FPlantRotorVisual V;V.Actor=*It;V.Initial=It->GetActorTransform();if(auto*Root=It->GetRootComponent())Root->SetMobility(EComponentMobility::Movable);Rotor.Add(V);}}}
void APowderPlantTest::ResetSimulation(){Stop();Error.Empty();PhysicalSeconds=ActualRate=0;Generation=0;try{
  if(!std::isfinite(FixedDt)||FixedDt<1e-6||FixedDt>2.5e-5||ContactIterations<4||ContactIterations>32||!std::isfinite(StageTimeoutSeconds)||StageTimeoutSeconds<=0)throw std::runtime_error("invalid plant test controls");
  powder::plant_discharge_ready(0,1,0,MinimumReceiverFraction,ReceiverTransferSeconds);
  FString Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/DataDirectory);auto P=ReadCharge(Root/TEXT("test_charge.csv"));ParticleCount=int32(P.size());int Types=0;for(auto A:P)Types=FMath::Max(Types,int(A.w)+1);
  while(Groups.Num()<Types){auto*G=NewObject<UInstancedStaticMeshComponent>(this);G->SetupAttachment(RootComponent);G->SetCollisionEnabled(ECollisionEnabled::NoCollision);G->SetCastShadow(false);G->RegisterComponent();AddInstanceComponent(G);Groups.Add(G);}
  auto*Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/DEM/SM_GrainSphere.SM_GrainSphere"));if(!Mesh)Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));auto*Mat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/M_BulkFlow.M_BulkFlow"));FLinearColor Colors[3]{FLinearColor(.9,.025,.01),FLinearColor(.01,.2,.9),FLinearColor(.9,.7,.01)};
  for(int I=0;I<Groups.Num();++I){auto*G=Groups[I].Get();G->ClearInstances();G->SetStaticMesh(Mesh);if(Mat){G->SetMaterial(0,Mat);if(auto*M=G->CreateDynamicMaterialInstance(0))M->SetVectorParameterValue(TEXT("Tint"),Colors[I%3]);}}
  double Diameter=Mesh->GetBounds().BoxExtent.GetMax()*2;for(auto A:P)Groups[int(A.w)]->AddInstance(FTransform(FQuat::Identity,FVector(A.x,-A.y,A.z)*100,FVector(.25/Diameter)));
  Stage=TEXT("Initial charge: 10000 per supplier");if(HasActorBegunPlay()){powder::GranularConfig C;C.plant=true;C.kernel_optimize=true;C.plant_data=powder::read_plant_data(TCHAR_TO_UTF8(*(Root/TEXT("runtime.plant"))));C.radius=.00125f;C.count=unsigned(P.size());C.dt=float(FixedDt);C.iterations=unsigned(ContactIterations);auto Routes=powder::read_plant_routes(TCHAR_TO_UTF8(*(Root/TEXT("runtime.routes"))));for(auto V:Rotor)if(V.Actor)V.Actor->SetActorTransform(V.Initial);Runtime=new FPlantRuntime(std::move(C),std::move(P),std::move(Routes),StageTimeoutSeconds,MinimumReceiverFraction,ReceiverTransferSeconds,Running,TargetRate,TestPressureDropPa,MixerRPM);}
 }catch(const std::exception&E){Error=UTF8_TO_TCHAR(E.what());Running=false;}}
void APowderPlantTest::BeginPlay(){Super::BeginPlay();if(auto*FPS=IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))){PreviousMaxFPS=FPS->GetFloat();FPS->Set(30.f,ECVF_SetByCode);}FParse::Value(FCommandLine::Get(),TEXT("PlantSmokeSeconds="),SmokeSeconds);FParse::Value(FCommandLine::Get(),TEXT("PlantWallLimit="),SmokeWallLimit);SmokeStart=FPlatformTime::Seconds();CacheRotor();ValveVisuals.Empty();for(TActorIterator<AActor>It(GetWorld());It;++It){if(auto*C=Cast<APlantVisibilityController>(*It))Display=C;const FString N=It->GetActorNameOrLabel();if((N.StartsWith(TEXT("V101"))&&N.EndsWith(TEXT("_Disc")))||N==TEXT("ASSUMED_VT_MovingGate")||N==TEXT("BV272_210_Disc")||N==TEXT("BV272_212_Disc_hub")||N==TEXT("BV122_210_Disc")||N==TEXT("BV122_212_Disc_hub"))ValveVisuals.Add(*It);}ResetSimulation();}
void APowderPlantTest::Show(){if(!Runtime)return;std::lock_guard<std::mutex>L(Runtime->Mutex);if(!Runtime->Error.empty()){Error=UTF8_TO_TCHAR(Runtime->Error.c_str());Running=false;}if(Generation==Runtime->Generation)return;Generation=Runtime->Generation;PhysicalSeconds=Runtime->Time;ActualRate=Runtime->Wall>0?PhysicalSeconds/Runtime->Wall:0;InWeighingHopper=Runtime->WH;InReceiver=Runtime->RH;InMixer=Runtime->MX;Stage=Runtime->Phase==0?TEXT("Supply -> weighing"):Runtime->Phase==1?TEXT("Weighing -> receiver"):TEXT("Receiver -> mixer");
 TArray<TArray<FTransform>> T;T.SetNum(Groups.Num());for(auto A:Runtime->Points){auto*G=Groups[int(A.w)].Get();double Diameter=G->GetStaticMesh()->GetBounds().BoxExtent.GetMax()*2;T[int(A.w)].Add(FTransform(FQuat::Identity,FVector(A.x,-A.y,A.z)*100,FVector(.25/Diameter)));}for(int I=0;I<Groups.Num();++I)Groups[I]->BatchUpdateInstancesTransforms(0,T[I],false,true,true);
 for(auto V:ValveVisuals)if(V){const FString N=V->GetActorNameOrLabel();bool Open=N.StartsWith(TEXT("BV122"))||(N.StartsWith(TEXT("V101"))&&Runtime->Phase==0)||(N.StartsWith(TEXT("ASSUMED"))&&Runtime->Phase==1)||(N.StartsWith(TEXT("BV272"))&&Runtime->Phase==2);V->SetActorHiddenInGame(Open||(Display&&Display->ViewMode==EPlantViewMode::Hidden));}
 FQuat Q(FVector(1,0,0),-Runtime->Angle);FVector Pivot(300,0,90);for(auto V:Rotor)if(V.Actor){FTransform X=V.Initial;X.SetLocation(Pivot+Q.RotateVector(V.Initial.GetLocation()-Pivot));X.SetRotation(Q*V.Initial.GetRotation());V.Actor->SetActorTransform(X);}}
void APowderPlantTest::Tick(float D){Super::Tick(D);if(auto*PC=GetWorld()->GetFirstPlayerController()){if(PC->WasInputKeyJustPressed(EKeys::SpaceBar))Running=!Running;if(PC->WasInputKeyJustPressed(EKeys::R))ResetSimulation();if(PC->WasInputKeyJustPressed(EKeys::O))ExportSnapshot();if(auto*Camera=Cast<ACameraActor>(PC->GetViewTarget())){int View=-1;if(PC->WasInputKeyJustPressed(EKeys::One))View=0;if(PC->WasInputKeyJustPressed(EKeys::Two))View=1;if(PC->WasInputKeyJustPressed(EKeys::Three))View=2;if(PC->WasInputKeyJustPressed(EKeys::Four))View=3;if(View>=0){FVector Targets[]{FVector(0,-140,260),FVector(0,0,115),FVector(300,0,275),FVector(300,0,90)};FVector Target=Targets[View];Camera->SetActorLocation(Target+FVector(80,View==0?300:150,60));Camera->SetActorRotation((Target-Camera->GetActorLocation()).Rotation());}}}if(Runtime){Runtime->Run=Running;Runtime->Rate=TargetRate;Runtime->Drop=TestPressureDropPa;Runtime->RPM=MixerRPM;}LastShow+=D;if(LastShow>.1){Show();LastShow=0;}if(GEngine)GEngine->AddOnScreenDebugMessage(3101,0,Error.IsEmpty()?FColor::Cyan:FColor::Red,FString::Printf(TEXT("PLANT TEST | 1D pressure + 3D GPU grains (one-way, uncalibrated)\nN %d | t %.3fs | %.4fx | %s | WH %d RH %d Mixer %d Other %d\n1 supply / 2 weighing / 3 receiver / 4 mixer | Space pause / R reset / O export | %s"),ParticleCount,PhysicalSeconds,ActualRate,*Stage,InWeighingHopper,InReceiver,InMixer,ParticleCount-InWeighingHopper-InReceiver-InMixer,*Error));if(SmokeSeconds>0){if(!SmokeShot&&PhysicalSeconds>=SmokeSeconds*.4){if(auto*PC=GetWorld()->GetFirstPlayerController())if(auto*Camera=Cast<ACameraActor>(PC->GetViewTarget())){FVector Target(0,-140,260);Camera->SetActorLocation(Target+FVector(30,300,60));Camera->SetActorRotation((Target-Camera->GetActorLocation()).Rotation());}FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("PlantFlowPreview.png"),true,false);SmokeShot=true;}
 if(PhysicalSeconds>=SmokeSeconds||!Error.IsEmpty()||FPlatformTime::Seconds()-SmokeStart>SmokeWallLimit){ExportSnapshot();FString Report=FString::Printf(TEXT("{\"count\":%d,\"physical_s\":%.9g,\"actual_rate\":%.9g,\"wh\":%d,\"rh\":%d,\"mixer\":%d,\"failed\":%s}"),ParticleCount,PhysicalSeconds,ActualRate,InWeighingHopper,InReceiver,InMixer,Error.IsEmpty()&&PhysicalSeconds>=SmokeSeconds?TEXT("false"):TEXT("true"));FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("PlantSmoke.json")));if(!Error.IsEmpty())FFileHelper::SaveStringToFile(Error,*(FPaths::ProjectSavedDir()/TEXT("PlantSmokeError.txt")));FPlatformMisc::RequestExitWithStatus(false,Error.IsEmpty()&&PhysicalSeconds>=SmokeSeconds?0:1);SmokeSeconds=0;}}}
void APowderPlantTest::ExportSnapshot(){if(!Runtime)return;std::lock_guard<std::mutex>L(Runtime->Mutex);std::ostringstream S;S<<"id,species,x_m,y_m,z_m\n";unsigned Id=0;for(auto A:Runtime->Points)S<<Id++<<','<<A.w<<','<<A.x<<','<<A.y<<','<<A.z<<'\n';FFileHelper::SaveStringToFile(UTF8_TO_TCHAR(S.str().c_str()),*(FPaths::ProjectSavedDir()/TEXT("PlantSnapshot.csv")));}
void APowderPlantTest::EndPlay(const EEndPlayReason::Type Reason){Stop();if(auto*FPS=IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))if(FPS->GetFloat()==30.f)FPS->Set(PreviousMaxFPS,ECVF_SetByCode);Super::EndPlay(Reason);}
