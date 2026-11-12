#include "PowderPlantTest.h"
#include "PlantVisibilityController.h"
#include "ProcessUIStateAdapter.h"
#include "ProcessControlService.h"
#include "PlantTransferTelemetry.h"
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
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <sstream>
#include <chrono>
namespace {
std::vector<powder::Grain> ReadCharge(const FString& Path,float& Radius,TArray<int32>& InitialCounts){
 InitialCounts.Init(0,3);
 FString Text;if(!FFileHelper::LoadFileToString(Text,*Path))throw std::runtime_error("particle charge file missing");
 std::istringstream File(TCHAR_TO_UTF8(*Text));std::string Line;std::getline(File,Line);std::vector<powder::Grain> P;
 while(std::getline(File,Line)){std::replace(Line.begin(),Line.end(),',',' ');std::istringstream Row(Line);unsigned Id,Type;std::string Supplier;float X,Y,Z,R,D;
  if(!(Row>>Id>>Type>>Supplier>>X>>Y>>Z>>R>>D)||Id!=P.size()||!std::isfinite(R)||R<=.001f||R>=.01f||D!=1200||Type>=32)throw std::runtime_error("invalid virtual charge; radius must be 1..10mm and density 1200");if(P.empty())Radius=R;else if(std::abs(R-Radius)>1e-8f)throw std::runtime_error("plant solver requires uniform particle radius");P.push_back({X,Y,Z,float(Type)});
  if(Supplier.size()==6&&Supplier.substr(0,5)=="H-101"&&Supplier[5]>='A'&&Supplier[5]<='C')++InitialCounts[Supplier[5]-'A'];
 }if(P.empty())throw std::runtime_error("empty particle charge");return P;
}
void Count(const std::vector<powder::Grain>&P,int&WH,int&RH,int&MX){WH=RH=MX=0;for(auto A:P){if(std::hypot(A.x,A.y)<.355&&A.z>.603&&A.z<1.78)++WH;if(std::hypot(A.x-3,A.y)<.255&&A.z>2.163&&A.z<3.7)++RH;if(A.x>2.25&&A.x<3.75&&std::abs(A.y)<.45&&A.z>.57&&A.z<1.31)++MX;}}
}
struct FPlantRuntime {
 FPlantSimulationSnapshot Published;
 std::thread Thread;std::mutex Mutex;std::atomic<bool> Quit{false};
 FPlantControlTargets Mailbox;
 std::vector<powder::Grain> Points;std::string Error;double Time=0,Wall=0,Angle=0;
 int Phase=0,WH=0,RH=0,MX=0;bool HopperOpen=false,ReceiverOpen=false;uint32 OpenSuppliers=0;uint64 Generation=1;
 FPlantRuntime(powder::GranularConfig C,std::vector<powder::Grain> P,std::vector<powder::PlantRoute> Routes,double Timeout,double Fraction,double TransferWait,bool Start,double Pressure,double Speed,FPlantTransferSettings TransferSettings):Points(P){
  Mailbox.Running=Start;Mailbox.Pressure=Pressure;Mailbox.RPM=15;
  Thread=std::thread([this,C=std::move(C),P=std::move(P),Routes=std::move(Routes),Timeout,Fraction,TransferWait,TransferSettings]()mutable{
   try{
    FPlantControlTargets Control;{std::lock_guard<std::mutex>L(Mutex);Control=Mailbox;}
    FPlantTransferTelemetry Telemetry(C,Routes,TransferSettings);
    int64 ObservationConnection=Control.ObservationConnection;Telemetry.ResetConnection(ObservationConnection);
    Telemetry.Observe(P,0,false,false);
    double AppliedPressure=Control.Pressure;auto Gates=powder::plant_supplier_gates(C.plant_data);
    int HopperGate=powder::plant_weighing_gate(C.plant_data),ReceiverGate=powder::plant_receiver_gate(C.plant_data);bool OpenHopper=false,PreviousHopper=false,OpenReceiver=false,PreviousReceiver=false;
    uint32 Open=0,PreviousRequest=0,All=Gates.size()==32?~uint32(0):(uint32(1)<<Gates.size())-1;
    powder::plant_set_manual_valves(C.plant_data,Routes,AppliedPressure,0,0,false,false);
    powder::plant_set_receiver_valve(C.plant_data,false);
    powder::GranularGPU GPU(C,P,std::vector<powder::Grain>(P.size()));
    const auto WallMemory=GPU.plant_memory_bytes();UE_LOG(LogTemp,Display,TEXT("Plant collision mode=%u particles=%u plant_bytes=%llu acceleration_bytes=%llu cache_bytes=%llu"),C.plant_wall_mode,C.count,(unsigned long long)WallMemory[0],(unsigned long long)WallMemory[1],(unsigned long long)WallMemory[2]);
    int PhaseNow=0;double StageStart=0,Active=0;
    double LastAppliedRPM=0;bool HasAppliedRPM=false,WasPaused=false;
    int TransferFlowCell=-1;for(const auto& Route:Routes)if(Route.phase==1&&!Route.cells.empty())TransferFlowCell=Route.cells.front()+2;
    // Telemetry only: called under Mutex, with results from a completed GPU batch.
    auto Publish=[&](EPlantExecutionState Execution){
     Published.Validity=EPlantValueValidity::Valid;Published.Execution=Execution;
     Published.PhysicalSeconds=Time;Published.InWeighingHopper=WH;Published.InReceiver=RH;Published.InMixer=MX;Telemetry.Publish(Published);
     if(TransferFlowCell>=0&&size_t(TransferFlowCell)<C.plant_data.size()){
      Published.Transfer.CalculatedCarrierFlowM3s=C.plant_data[TransferFlowCell].x;
      Published.Transfer.CarrierFlowValid=FMath::IsFinite(Published.Transfer.CalculatedCarrierFlowM3s)&&Published.Transfer.CalculatedCarrierFlowM3s>=0;
     }
     Published.AppliedSupplierMask=int32(OpenSuppliers);Published.AppliedHopperOpen=HopperOpen;Published.AppliedReceiverOpen=ReceiverOpen;
     Published.AppliedRPM=LastAppliedRPM;Published.AppliedRPMValid=HasAppliedRPM;
     Published.ProcessedControlRevision=Control.Revision;Published.WorkerTargetRPM=Control.RPM;Published.WorkerMixerEnabled=Control.MixerEnabled;
     Published.UpdatedUtc=FDateTime::UtcNow();Published.PublishedWallSeconds=FPlatformTime::Seconds();++Published.Sequence;
    };
    {std::lock_guard<std::mutex>L(Mutex);Count(Points,WH,RH,MX);Publish(Control.Running?EPlantExecutionState::Running:EPlantExecutionState::Paused);}
    auto LastPublish=std::chrono::steady_clock::now(),LastControl=LastPublish;
    while(!Quit){
     {std::lock_guard<std::mutex>L(Mutex);Control=Mailbox;}
     if(Control.ObservationConnection!=ObservationConnection){ObservationConnection=Control.ObservationConnection;Telemetry.ResetConnection(ObservationConnection);Telemetry.Observe(GPU.snapshot(),GPU.time(),OpenHopper,Control.ReceiverOpen||OpenReceiver);}
     if(Control.ReceiverOpen||OpenReceiver)Telemetry.ReceiverDischarging();
     if(!Control.Running){
      auto Now=std::chrono::steady_clock::now();
      if(!WasPaused||std::chrono::duration<double>(Now-LastPublish).count()>.1){
       // Capture the final completed batch once on pause; heartbeats do not step the solver.
       if(!WasPaused){auto Final=GPU.snapshot();Telemetry.Observe(Final,GPU.time(),OpenHopper,Control.ReceiverOpen||OpenReceiver);std::lock_guard<std::mutex>L(Mutex);Points=std::move(Final);Time=GPU.time();Angle=GPU.angle();Wall=Active;Count(Points,WH,RH,MX);OpenSuppliers=Open;HopperOpen=OpenHopper;ReceiverOpen=OpenReceiver;++Generation;}
       {std::lock_guard<std::mutex>L(Mutex);Publish(EPlantExecutionState::Paused);}LastPublish=Now;
      }
      WasPaused=true;std::this_thread::sleep_for(std::chrono::milliseconds(5));continue;
     }
     WasPaused=false;
     auto Begin=std::chrono::steady_clock::now();double S=Control.MixerEnabled?Control.RPM:0,D=Control.Pressure;
     if(!std::isfinite(Control.RPM)||Control.RPM<PlantControl::MinRPM||Control.RPM>PlantControl::MaxRPM)throw std::runtime_error("invalid live RPM");
     bool WantHopper=Control.WeighingOpen,WantReceiver=Control.ReceiverOpen;
     uint32 Request=Control.Suppliers&All;
     if(D!=AppliedPressure||Request!=PreviousRequest||WantHopper!=PreviousHopper||WantReceiver!=PreviousReceiver||((Open!=Request||OpenHopper!=WantHopper||OpenReceiver!=WantReceiver)&&std::chrono::duration<double>(Begin-LastControl).count()>.1)){
      uint32 NextOpen=Open|Request,Closing=Open&~Request;bool NextHopper=OpenHopper||WantHopper,NextReceiver=OpenReceiver||WantReceiver;
      if(Closing||(OpenHopper&&!WantHopper)||(OpenReceiver&&!WantReceiver)){auto Current=GPU.snapshot();if(OpenReceiver&&!WantReceiver&&powder::plant_supplier_closure_clear(C.plant_data[ReceiverGate],Current,C.radius))NextReceiver=false;if(OpenHopper&&!WantHopper&&powder::plant_supplier_closure_clear(C.plant_data[HopperGate],Current,C.radius))NextHopper=false;for(size_t i=0;i<Gates.size();++i)if((Closing&(uint32(1)<<i))&&powder::plant_supplier_closure_clear(C.plant_data[Gates[i]],Current,C.radius))NextOpen&=~(uint32(1)<<i);}
      if(NextOpen!=Open||Request!=PreviousRequest||D!=AppliedPressure||NextHopper!=OpenHopper||WantHopper!=PreviousHopper||NextReceiver!=OpenReceiver||WantReceiver!=PreviousReceiver){
       if(!OpenHopper&&NextHopper){auto BeforeOpen=GPU.snapshot();Telemetry.Begin(BeforeOpen,GPU.time(),Control.Revision,Control.ReceiverOpen||OpenReceiver);}
       int NextPhase=NextHopper?1:0;if(NextPhase!=PhaseNow){PhaseNow=NextPhase;StageStart=GPU.time();C.plant_data[4].x=float(PhaseNow);}
       powder::plant_set_manual_valves(C.plant_data,Routes,D,NextOpen,Request,NextHopper,WantHopper);powder::plant_set_receiver_valve(C.plant_data,NextReceiver);GPU.update_plant_field(C.plant_data);
       OpenReceiver=NextReceiver;PreviousReceiver=WantReceiver;Open=NextOpen;OpenHopper=NextHopper;PreviousHopper=WantHopper;PreviousRequest=Request;AppliedPressure=D;
      }LastControl=Begin;
     }
     GPU.advance(16,float(S),false);
     LastAppliedRPM=double(float(S));HasAppliedRPM=true;
     // No wall-clock pacing: run the next fixed-dt batch as soon as this one completes.
     Active+=std::chrono::duration<double>(std::chrono::steady_clock::now()-Begin).count();
     if(PhaseNow==0||(PhaseNow==1&&!WantHopper))StageStart=GPU.time(); // An intentional closed supply is not a timeout.
     if(std::chrono::duration<double>(std::chrono::steady_clock::now()-LastPublish).count()>.1){
      auto Next=GPU.snapshot();Telemetry.Observe(Next,GPU.time(),OpenHopper,Control.ReceiverOpen||OpenReceiver);int W,H,M;Count(Next,W,H,M);
      {std::lock_guard<std::mutex>L(Mutex);Points=std::move(Next);Time=GPU.time();Angle=GPU.angle();Wall=Active;WH=W;RH=H;MX=M;Phase=PhaseNow;OpenSuppliers=Open;HopperOpen=OpenHopper;ReceiverOpen=OpenReceiver;++Generation;Publish(EPlantExecutionState::Running);}
      // Manual valve operation may intentionally retain material indefinitely.
      LastPublish=std::chrono::steady_clock::now();
     }
    }
   }catch(const std::exception&E){std::lock_guard<std::mutex>L(Mutex);Error=E.what();Published.Error=UTF8_TO_TCHAR(E.what());Published.Validity=EPlantValueValidity::Error;Published.Execution=EPlantExecutionState::Fault;Published.UpdatedUtc=FDateTime::UtcNow();Published.PublishedWallSeconds=FPlatformTime::Seconds();++Published.Sequence;}
  });
 }
 ~FPlantRuntime(){Quit=true;if(Thread.joinable())Thread.join();}
};
APowderPlantTest::APowderPlantTest(){PrimaryActorTick.bCanEverTick=true;RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));}
APowderPlantTest::~APowderPlantTest(){Stop();}
void APowderPlantTest::Stop(){delete Runtime;Runtime=nullptr;}
void APowderPlantTest::CacheRotor(){Rotor.Empty();for(TActorIterator<AActor>It(GetWorld());It;++It){const FString N=It->GetActorNameOrLabel();if(N.StartsWith(TEXT("MX-301_"))&&(N.Contains(TEXT("Paddle"))||N.Contains(TEXT("Shaft")))&&!N.Contains(TEXT("Drive"))&&!N.Contains(TEXT("Support"))){FPlantRotorVisual V;V.Actor=*It;V.Initial=It->GetActorTransform();if(auto*Root=It->GetRootComponent())Root->SetMobility(EComponentMobility::Movable);Rotor.Add(V);}}}
void APowderPlantTest::ResetSimulation(){ControlTargets=FPlantControlTargets();++SnapshotEpoch;InitialSupplierCounts.Empty();Stop();Error.Empty();PhysicalSeconds=ActualRate=0;Generation=0;CurrentPhase=0;AppliedSupplierMask=0;WeighingHopperOpen=AppliedHopperOpen=false;ReceiverOpen=AppliedReceiverOpen=MixerEnabled=false;InWeighingHopper=InReceiver=InMixer=0;try{
  InitializeSuppliers();for(int i=0;i<SupplierOpen.Num();++i)SupplierOpen[i]=false;
  if(!std::isfinite(FixedDt)||FixedDt<1e-6||FixedDt>2.5e-5||ContactIterations<4||ContactIterations>32||!std::isfinite(StageTimeoutSeconds)||StageTimeoutSeconds<=0)throw std::runtime_error("invalid plant test controls");
  powder::plant_discharge_ready(0,1,0,MinimumReceiverFraction,ReceiverTransferSeconds);
  FString Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/DataDirectory);auto P=ReadCharge(Root/TEXT("test_charge.csv"),ParticleRadiusM,InitialSupplierCounts);ParticleCount=int32(P.size());int Types=0;for(auto A:P)Types=FMath::Max(Types,int(A.w)+1);
  while(Groups.Num()<Types){auto*G=NewObject<UInstancedStaticMeshComponent>(this);G->SetupAttachment(RootComponent);G->SetCollisionEnabled(ECollisionEnabled::NoCollision);G->SetCastShadow(false);G->RegisterComponent();AddInstanceComponent(G);Groups.Add(G);}
  auto*Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/DEM/SM_GrainSphere.SM_GrainSphere"));if(!Mesh)Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));auto*Mat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/M_BulkFlow.M_BulkFlow"));FLinearColor Colors[3]{FLinearColor(.9,.025,.01),FLinearColor(.01,.2,.9),FLinearColor(.9,.7,.01)};
  for(int I=0;I<Groups.Num();++I){auto*G=Groups[I].Get();G->ClearInstances();G->SetStaticMesh(Mesh);if(Mat){G->SetMaterial(0,Mat);if(auto*M=G->CreateDynamicMaterialInstance(0))M->SetVectorParameterValue(TEXT("Tint"),Colors[I%3]);}}
  double Diameter=Mesh->GetBounds().BoxExtent.GetMax()*2;for(auto A:P)Groups[int(A.w)]->AddInstance(FTransform(FQuat::Identity,FVector(A.x,-A.y,A.z)*100,FVector((200.*ParticleRadiusM)/Diameter)));
  Stage=FString::Printf(TEXT("Initial charge: %d particles"),ParticleCount);if(HasActorBegunPlay()){powder::GranularConfig C;C.plant=true;C.plant_threaded=true;C.kernel_optimize=true;C.plant_data=powder::read_plant_data(TCHAR_TO_UTF8(*(Root/TEXT("runtime.plant"))));C.plant_wall_mode=(C.plant_data[10].x==2&&C.plant_data[13].z>0&&C.plant_data[13].w>0)?(P.size()>=3000?5u:3u):0u;if(int(C.plant_data[11].x)!=SectionTest)throw std::runtime_error("section mode/data mismatch");C.radius=ParticleRadiusM;C.count=unsigned(P.size());C.dt=float(FixedDt);C.iterations=unsigned(ContactIterations);auto Routes=powder::read_plant_routes(TCHAR_TO_UTF8(*(Root/TEXT("runtime.routes"))));for(auto V:Rotor)if(V.Actor)V.Actor->SetActorTransform(V.Initial);ControlTargets.Running=Running;ControlTargets.RPM=MixerRPM;ControlTargets.Pressure=TestPressureDropPa;Runtime=new FPlantRuntime(std::move(C),std::move(P),std::move(Routes),StageTimeoutSeconds,MinimumReceiverFraction,ReceiverTransferSeconds,Running,TestPressureDropPa,0,TransferSettings);PublishControls();}
 }catch(const std::exception&E){Error=UTF8_TO_TCHAR(E.what());Running=false;}}
void APowderPlantTest::BeginPlay(){Super::BeginPlay();if(GEngine)GEngine->RemoveOnScreenDebugMessage(3101);if(auto*FPS=IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))){PreviousMaxFPS=FPS->GetFloat();FPS->Set(30.f,ECVF_SetByCode);}FParse::Value(FCommandLine::Get(),TEXT("PlantSmokeSeconds="),SmokeSeconds);FParse::Value(FCommandLine::Get(),TEXT("PlantWallLimit="),SmokeWallLimit);SmokeStart=FPlatformTime::Seconds();CacheRotor();ValveVisuals.Empty();for(TActorIterator<AActor>It(GetWorld());It;++It){if(auto*C=Cast<APlantVisibilityController>(*It))Display=C;const FString N=It->GetActorNameOrLabel();if((N.StartsWith(TEXT("V101"))&&N.EndsWith(TEXT("_Disc")))||N==TEXT("ASSUMED_VT_MovingGate")||N==TEXT("BV272_210_Disc")||N==TEXT("BV272_212_Disc_hub")||N==TEXT("BV122_210_Disc")||N==TEXT("BV122_212_Disc_hub"))ValveVisuals.Add(*It);}ResetSimulation();}
void APowderPlantTest::Show(){if(!Runtime)return;std::lock_guard<std::mutex>L(Runtime->Mutex);if(!Runtime->Error.empty()){Error=UTF8_TO_TCHAR(Runtime->Error.c_str());Running=false;}if(Generation==Runtime->Generation)return;Generation=Runtime->Generation;PhysicalSeconds=Runtime->Time;ActualRate=Runtime->Wall>0?PhysicalSeconds/Runtime->Wall:0;InWeighingHopper=Runtime->WH;InReceiver=Runtime->RH;InMixer=Runtime->MX;CurrentPhase=Runtime->Phase;AppliedSupplierMask=Runtime->OpenSuppliers;AppliedHopperOpen=Runtime->HopperOpen;AppliedReceiverOpen=Runtime->ReceiverOpen;Stage=Runtime->Phase==0?TEXT("공급기 → 칭량 호퍼"):Runtime->Phase==1?TEXT("공급기 → 칭량 → 수취부 (연결 유동)"):TEXT("수신기 → 혼합기");
 TArray<TArray<FTransform>> T;T.SetNum(Groups.Num());for(auto A:Runtime->Points){auto*G=Groups[int(A.w)].Get();double Diameter=G->GetStaticMesh()->GetBounds().BoxExtent.GetMax()*2;T[int(A.w)].Add(FTransform(FQuat::Identity,FVector(A.x,-A.y,A.z)*100,FVector((200.*ParticleRadiusM)/Diameter)));}for(int I=0;I<Groups.Num();++I)Groups[I]->BatchUpdateInstancesTransforms(0,T[I],false,true,true);
 for(auto V:ValveVisuals)if(V){const FString N=V->GetActorNameOrLabel();const int SupplierIndex=N.Len()>4?int(N[4]-TCHAR('A')):-1;bool Open=N.StartsWith(TEXT("BV122"))||(N.StartsWith(TEXT("V101"))&&SupplierIndex>=0&&SupplierIndex<SupplierOpen.Num()&&Runtime->Phase<2&&((Runtime->OpenSuppliers&(uint32(1)<<SupplierIndex))!=0))||(N.StartsWith(TEXT("ASSUMED"))&&Runtime->HopperOpen&&Runtime->Phase<2)||(N.StartsWith(TEXT("BV272"))&&Runtime->ReceiverOpen);V->SetActorHiddenInGame(Open||(Display&&Display->ViewMode==EPlantViewMode::Hidden));}
 FQuat Q(FVector(1,0,0),-Runtime->Angle);FVector Pivot(300,0,90);for(auto V:Rotor)if(V.Actor){FTransform X=V.Initial;X.SetLocation(Pivot+Q.RotateVector(V.Initial.GetLocation()-Pivot));X.SetRotation(Q*V.Initial.GetRotation());V.Actor->SetActorTransform(X);}}
void APowderPlantTest::Tick(float D){Super::Tick(D);if(auto*PC=GetWorld()->GetFirstPlayerController()){const auto* UI=GetWorld()->GetSubsystem<UProcessUIStateSubsystem>();if(LegacyControlsAllowed()&&(!UI||!UI->IsReadOnlyScreenActive())){if(PC->WasInputKeyJustPressed(EKeys::SpaceBar))Running=!Running;if(PC->WasInputKeyJustPressed(EKeys::R))ResetSimulation();if(PC->WasInputKeyJustPressed(EKeys::O))ExportSnapshot();}if(auto*Camera=Cast<ACameraActor>(PC->GetViewTarget())){int View=-1;if(PC->WasInputKeyJustPressed(EKeys::One))View=0;if(PC->WasInputKeyJustPressed(EKeys::Two))View=1;if(PC->WasInputKeyJustPressed(EKeys::Three))View=2;if(PC->WasInputKeyJustPressed(EKeys::Four))View=3;if(View>=0){FVector Targets[]{FVector(0,-140,260),FVector(0,0,115),FVector(300,0,275),FVector(300,0,90)};FVector Target=Targets[View];Camera->SetActorLocation(Target+FVector(80,View==0?300:150,60));Camera->SetActorRotation((Target-Camera->GetActorLocation()).Rotation());}}}if(Runtime){if(ControlOwner.IsValid()){Running=ControlTargets.Running;MixerRPM=ControlTargets.RPM;MixerEnabled=ControlTargets.MixerEnabled;WeighingHopperOpen=ControlTargets.WeighingOpen;ReceiverOpen=ControlTargets.ReceiverOpen;for(int i=0;i<SupplierOpen.Num();++i)SupplierOpen[i]=(ControlTargets.Suppliers&(uint32(1)<<i))!=0;}else{ControlTargets.Running=Running;ControlTargets.Pressure=TestPressureDropPa;ControlTargets.RPM=MixerRPM;ControlTargets.MixerEnabled=MixerEnabled;ControlTargets.Suppliers=0;for(int i=0;i<SupplierOpen.Num();++i)if(SupplierOpen[i])ControlTargets.Suppliers|=uint32(1)<<i;ControlTargets.WeighingOpen=WeighingHopperOpen;ControlTargets.ReceiverOpen=ReceiverOpen;}PublishControls();}LastShow+=D;if(LastShow>.1){Show();LastShow=0;}if(SmokeSeconds>0){if(!SmokeShot&&PhysicalSeconds>=SmokeSeconds*.4){if(auto*PC=GetWorld()->GetFirstPlayerController())if(auto*Camera=Cast<ACameraActor>(PC->GetViewTarget())){FVector Target(0,-140,260);Camera->SetActorLocation(Target+FVector(30,300,60));Camera->SetActorRotation((Target-Camera->GetActorLocation()).Rotation());}FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("PlantFlowPreview.png"),true,false);SmokeShot=true;}
 if(PhysicalSeconds>=SmokeSeconds||!Error.IsEmpty()||FPlatformTime::Seconds()-SmokeStart>SmokeWallLimit){ExportSnapshot();FString Report=FString::Printf(TEXT("{\"count\":%d,\"physical_s\":%.9g,\"actual_rate\":%.9g,\"wh\":%d,\"rh\":%d,\"mixer\":%d,\"failed\":%s}"),ParticleCount,PhysicalSeconds,ActualRate,InWeighingHopper,InReceiver,InMixer,Error.IsEmpty()&&PhysicalSeconds>=SmokeSeconds?TEXT("false"):TEXT("true"));FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("PlantSmoke.json")));if(!Error.IsEmpty())FFileHelper::SaveStringToFile(Error,*(FPaths::ProjectSavedDir()/TEXT("PlantSmokeError.txt")));FPlatformMisc::RequestExitWithStatus(false,Error.IsEmpty()&&PhysicalSeconds>=SmokeSeconds?0:1);SmokeSeconds=0;}}}
void APowderPlantTest::ExportSnapshot(){if(!Runtime)return;std::lock_guard<std::mutex>L(Runtime->Mutex);std::ostringstream S;S<<"id,species,x_m,y_m,z_m\n";unsigned Id=0;for(auto A:Runtime->Points)S<<Id++<<','<<A.w<<','<<A.x<<','<<A.y<<','<<A.z<<'\n';FFileHelper::SaveStringToFile(UTF8_TO_TCHAR(S.str().c_str()),*(FPaths::ProjectSavedDir()/TEXT("PlantSnapshot.csv")));}
void APowderPlantTest::EndPlay(const EEndPlayReason::Type Reason){Stop();if(auto*FPS=IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))if(FPS->GetFloat()==30.f)FPS->Set(PreviousMaxFPS,ECVF_SetByCode);Super::EndPlay(Reason);}

void APowderPlantTest::InitializeSuppliers(){
 FString Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/DataDirectory);
 auto Data=powder::read_plant_data(TCHAR_TO_UTF8(*(Root/TEXT("runtime.plant"))));
 SupplierOpen.Init(false,int32(powder::plant_supplier_gates(Data).size()));
}
void APowderPlantTest::ToggleSupplier(int32 Index){
 if(LegacyControlsAllowed()&&HasActorBegunPlay()&&CurrentPhase<2&&Error.IsEmpty()&&SupplierOpen.IsValidIndex(Index))SupplierOpen[Index]=!SupplierOpen[Index];
}
void APowderPlantTest::ToggleWeighingHopper(){
 if(LegacyControlsAllowed()&&HasActorBegunPlay()&&CurrentPhase<2&&Error.IsEmpty())WeighingHopperOpen=!WeighingHopperOpen;
}
void APowderPlantTest::ToggleReceiver(){if(LegacyControlsAllowed()&&HasActorBegunPlay()&&Error.IsEmpty())ReceiverOpen=!ReceiverOpen;}
void APowderPlantTest::ToggleMixer(){if(LegacyControlsAllowed()&&HasActorBegunPlay()&&Error.IsEmpty())MixerEnabled=!MixerEnabled;}
TSharedRef<SWidget> APowderPlantTest::BuildSimulationPanel(){
 if(SupplierOpen.IsEmpty()){try{InitializeSuppliers();}catch(const std::exception&E){Error=UTF8_TO_TCHAR(E.what());}}
 const TWeakObjectPtr<APowderPlantTest> Weak(this);
 TSharedRef<SVerticalBox> Panel=SNew(SVerticalBox);
 if(SectionTest>0){const TCHAR*Names[]{TEXT(""),TEXT("공급기 → 칭량호퍼 | 칭량부 시험 흡입압"),TEXT("칭량호퍼 → 수취부 | 칭량부 대기압·수취부 시험 흡입압"),TEXT("수취부 → 혼합기 | 양쪽 대기압·중력 배출"),TEXT("혼합기 내부 | 공기 이송 없음")};Panel->AddSlot().AutoHeight()[SNew(STextBlock).WrapTextAt(350).Text(FText::FromString(Names[FMath::Clamp(SectionTest,0,4)]))];}
 if(DataDirectory.Contains(TEXT("ProcessPlantHybrid")))Panel->AddSlot().AutoHeight()[SNew(STextBlock).WrapTextAt(350).Text(FText::FromString(TEXT("혼합 충돌 시험: 관·연결부 메시 / 호퍼·혼합기 OBB")))];
 if(DataDirectory.Contains(TEXT("ProcessPlantOBB")))Panel->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("OBB 통로 근사 시험 · 원본 형상과 다름")))];
 Panel->AddSlot().AutoHeight().Padding(0,0,0,6)[SNew(STextBlock).Text(FText::FromString(TEXT("시뮬레이션 정보 · 배속 제한 없음")))];
 Panel->AddSlot().AutoHeight()[SNew(STextBlock).WrapTextAt(350).Text_Lambda([Weak](){
  if(!Weak.IsValid())return FText::GetEmpty();const auto*P=Weak.Get();
  return FText::FromString(FString::Printf(TEXT("시간 %.3f초 | 실제 속도 %.4f배\n입자 %d개 | %s\n칭량 %d · 수신기 %d · 혼합기 %d\n통로/기타 %d | %s"),P->PhysicalSeconds,P->ActualRate,P->ParticleCount,*(P->SectionTest>0?FString::Printf(TEXT("구간 %d 독립 시험"),P->SectionTest):P->Stage),P->InWeighingHopper,P->InReceiver,P->InMixer,P->ParticleCount-P->InWeighingHopper-P->InReceiver-P->InMixer,P->Running?TEXT("계산 중"):TEXT("일시정지")));
 })];
 if(SectionTest==0||SectionTest==1)Panel->AddSlot().AutoHeight().Padding(0,8,0,4)[SNew(STextBlock).Text(FText::FromString(TEXT("공급기 밸브 · 시작/초기화 시 모두 닫힘")))];
 for(int32 I=0;I<SupplierOpen.Num()&&(SectionTest==0||SectionTest==1);++I){
  Panel->AddSlot().AutoHeight().Padding(0,2)[SNew(SHorizontalBox)
   + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text_Lambda([Weak,I](){
    if(!Weak.IsValid()||!Weak->SupplierOpen.IsValidIndex(I))return FText::GetEmpty();
    bool Want=Weak->SupplierOpen[I],Open=(Weak->AppliedSupplierMask&(uint32(1)<<I))!=0;
    const TCHAR*State=Want?(Open?TEXT("열림"):TEXT("열림 대기")):(Open?TEXT("닫힘 대기 (입자 통과 중)"):TEXT("닫힘"));
    return FText::FromString(FString::Printf(TEXT("공급기 %c · %s"),TCHAR('A'+I),State));
   })]
   + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).IsFocusable(false).ContentPadding(FMargin(14,5))
    .IsEnabled_Lambda([Weak](){return Weak.IsValid()&&Weak->LegacyControlsAllowed()&&Weak->HasActorBegunPlay()&&Weak->CurrentPhase<2&&Weak->Error.IsEmpty();})
    .OnClicked_Lambda([Weak,I](){if(Weak.IsValid())Weak->ToggleSupplier(I);return FReply::Handled();})
    [SNew(STextBlock).Text_Lambda([Weak,I](){return FText::FromString(Weak.IsValid()&&Weak->SupplierOpen.IsValidIndex(I)&&Weak->SupplierOpen[I]?TEXT("닫기"):TEXT("열기"));})]]];
 }
 if(SectionTest==0||SectionTest==2)Panel->AddSlot().AutoHeight().Padding(0,8,0,0)[SNew(SHorizontalBox)
  + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text_Lambda([Weak](){
   if(!Weak.IsValid())return FText::GetEmpty();bool Want=Weak->WeighingHopperOpen,Open=Weak->AppliedHopperOpen;
   const TCHAR*State=Want?(Open?TEXT("열림"):TEXT("열림 대기")):(Open?TEXT("닫힘 대기 (입자 통과 중)"):TEXT("닫힘"));
   return FText::FromString(FString::Printf(TEXT("공용 칭량 호퍼 · %s"),State));
  })]
  + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).IsFocusable(false).ContentPadding(FMargin(14,5))
   .IsEnabled_Lambda([Weak](){return Weak.IsValid()&&Weak->LegacyControlsAllowed()&&Weak->HasActorBegunPlay()&&Weak->CurrentPhase<2&&Weak->Error.IsEmpty();})
   .OnClicked_Lambda([Weak](){if(Weak.IsValid())Weak->ToggleWeighingHopper();return FReply::Handled();})
   [SNew(STextBlock).Text_Lambda([Weak](){return FText::FromString(Weak.IsValid()&&Weak->WeighingHopperOpen?TEXT("닫기"):TEXT("열기"));})]]];
 for(int Control=0;Control<2;++Control){
  if(SectionTest!=0&&!(Control==0?SectionTest==3:(SectionTest==3||SectionTest==4)))continue;
  Panel->AddSlot().AutoHeight().Padding(0,6,0,0)[SNew(SHorizontalBox)
   + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text_Lambda([Weak,Control](){
    if(!Weak.IsValid())return FText::GetEmpty();
    if(Control==1)return FText::FromString(FString::Printf(TEXT("혼합기 · %s (%.1f RPM)"),Weak->MixerEnabled?TEXT("가동 요청"):TEXT("정지"),Weak->MixerRPM));
    bool Want=Weak->ReceiverOpen,Open=Weak->AppliedReceiverOpen;
    return FText::FromString(FString::Printf(TEXT("수취부 → 혼합기 · %s"),Want?(Open?TEXT("열림"):TEXT("열림 대기")):(Open?TEXT("닫힘 대기"):TEXT("닫힘"))));
   })]
   + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).IsFocusable(false).ContentPadding(FMargin(14,5))
    .IsEnabled_Lambda([Weak](){return Weak.IsValid()&&Weak->LegacyControlsAllowed()&&Weak->HasActorBegunPlay()&&Weak->Error.IsEmpty();})
    .OnClicked_Lambda([Weak,Control](){if(Weak.IsValid()){if(Control)Weak->ToggleMixer();else Weak->ToggleReceiver();}return FReply::Handled();})
    [SNew(STextBlock).Text_Lambda([Weak,Control](){return FText::FromString(Control?(Weak.IsValid()&&Weak->MixerEnabled?TEXT("정지"):TEXT("시작")):(Weak.IsValid()&&Weak->ReceiverOpen?TEXT("닫기"):TEXT("열기")));})]]];
 }
 Panel->AddSlot().AutoHeight().Padding(0,8,0,0)[SNew(STextBlock).WrapTextAt(350).Text(FText::FromString(TEXT("1~4: 장치 확대 | Space: 일시정지\nR: 초기화 | O: 입자 저장\n관로 압력 근사 + GPU 입자 시험 · 미보정")))];
 Panel->AddSlot().AutoHeight().Padding(0,4)[SNew(STextBlock).WrapTextAt(350).ColorAndOpacity(FLinearColor(1,.25,.2)).Text_Lambda([Weak](){return Weak.IsValid()?FText::FromString(Weak->Error):FText::GetEmpty();})];
 return SNew(SBox).WidthOverride(360)[Panel];
}

FPlantSimulationSnapshot APowderPlantTest::GetSimulationSnapshot() const {
 check(IsInGameThread());
 FPlantSimulationSnapshot Result;
 if(Runtime){std::lock_guard<std::mutex> Lock(Runtime->Mutex);Result=Runtime->Published;}
 else {Result.Validity=EPlantValueValidity::Initializing;Result.Execution=EPlantExecutionState::Initializing;}
 if(Result.Transfer.Connection!=ControlTargets.ObservationConnection){
  Result.WeighingMass.Validity=Result.PipeMass.Validity=Result.ReceiverMass.Validity=EPlantValueValidity::Unknown;
  Result.Transfer=FPlantTransferSnapshot();Result.Transfer.Connection=ControlTargets.ObservationConnection;
  Result.Transfer.State=EPlantTransferState::Indeterminate;Result.Transfer.Reason=TEXT("연결 변경으로 이전 이송 결과를 무효화했습니다. 새 확인값을 기다리세요.");
 }
 Result.Epoch=SnapshotEpoch;Result.SourceActorPath=GetPathName();Result.RequestedControlRevision=ControlTargets.Revision;
 Result.InitialSupplierCounts=InitialSupplierCounts;Result.RequestedSuppliers=SupplierOpen;
 Result.RequestedHopperOpen=WeighingHopperOpen;Result.RequestedReceiverOpen=ReceiverOpen;
 Result.RequestedRunning=Running;Result.RequestedMixerEnabled=MixerEnabled;Result.RequestedRPM=MixerRPM;
 if(!Error.IsEmpty()){Result.Error=Error;Result.Validity=EPlantValueValidity::Error;Result.Execution=EPlantExecutionState::Fault;}
 if(Result.Validity!=EPlantValueValidity::Valid){Result.WeighingMass.Validity=Result.PipeMass.Validity=Result.ReceiverMass.Validity=Result.Validity;Result.Transfer.Validity=Result.Validity;}
 if(Result.Validity==EPlantValueValidity::Valid){
  if(!Running&&Result.Execution==EPlantExecutionState::Running)Result.Execution=EPlantExecutionState::PauseRequested;
  if(Running&&Result.Execution==EPlantExecutionState::Paused)Result.Execution=EPlantExecutionState::ResumeRequested;
 }
 return Result;
}


bool APowderPlantTest::LegacyControlsAllowed() const { return !ControlOwner.IsValid(); }

bool APowderPlantTest::ClaimControl(UProcessControlService* ServiceOwner)
{
 check(IsInGameThread());
 if(ControlOwner.IsValid() && ControlOwner.Get()!=ServiceOwner)return false;
 if(!ControlOwner.IsValid()){
  ++ControlTargets.ObservationConnection;
  ControlTargets.Running=Running;ControlTargets.Pressure=TestPressureDropPa;ControlTargets.RPM=MixerRPM;
  ControlTargets.MixerEnabled=MixerEnabled;ControlTargets.WeighingOpen=WeighingHopperOpen;ControlTargets.ReceiverOpen=ReceiverOpen;
  ControlTargets.Suppliers=0;for(int32 I=0;I<SupplierOpen.Num();++I)if(SupplierOpen[I])ControlTargets.Suppliers|=uint32(1)<<I;
 }
 ControlOwner=ServiceOwner;PublishControls();return true;
}

void APowderPlantTest::ReleaseControl(UProcessControlService* ServiceOwner)
{
 check(IsInGameThread());
 if(ControlOwner.Get()!=ServiceOwner)return;
 ControlOwner.Reset();++ControlTargets.ObservationConnection;PublishControls();
}

bool APowderPlantTest::ValidateControl(int64 Epoch,FString& Reason) const
{
 check(IsInGameThread());
 if(!ControlOwner.IsValid()){Reason=TEXT("제어 서비스가 연결되지 않았습니다.");return false;}
 if(Epoch!=SnapshotEpoch){Reason=TEXT("초기화 세대가 변경되었습니다.");return false;}
 const auto State=GetSimulationSnapshot();
 if(!Runtime||!HasActorBegunPlay()||State.Validity!=EPlantValueValidity::Valid||State.Execution==EPlantExecutionState::Fault){Reason=TEXT("작업자를 사용할 수 없습니다. 오류 시 복구·초기화가 필요합니다.");return false;}
 if(SectionTest!=0||SupplierOpen.Num()!=3){Reason=TEXT("이 제어 연결은 전체 공정 A~C 구성만 지원합니다.");return false;}
 return true;
}

void APowderPlantTest::PublishControls()
{
 if(!Runtime)return;
 std::lock_guard<std::mutex> Lock(Runtime->Mutex);
 Runtime->Mailbox=ControlTargets;
}

bool APowderPlantTest::SetSupplierValve(int32 Index,bool Open,int64 Epoch,int64& Revision,FString& Reason)
{
 if(!ValidateControl(Epoch,Reason))return false;
 if(Index<0||Index>=3||!SupplierOpen.IsValidIndex(Index)){Reason=TEXT("지원하지 않는 공급기입니다.");return false;}
 SupplierOpen[Index]=Open;
 if(Open)ControlTargets.Suppliers|=uint32(1)<<Index;else ControlTargets.Suppliers&=~(uint32(1)<<Index);
 Revision=++ControlTargets.Revision;PublishControls();return true;
}
bool APowderPlantTest::SetWeighingValve(bool Open,int64 Epoch,int64& Revision,FString& Reason)
{
 if(!ValidateControl(Epoch,Reason))return false;
 WeighingHopperOpen=ControlTargets.WeighingOpen=Open;Revision=++ControlTargets.Revision;PublishControls();return true;
}
bool APowderPlantTest::SetReceiverValve(bool Open,int64 Epoch,int64& Revision,FString& Reason)
{
 if(!ValidateControl(Epoch,Reason))return false;
 ReceiverOpen=ControlTargets.ReceiverOpen=Open;Revision=++ControlTargets.Revision;PublishControls();return true;
}
bool APowderPlantTest::SetMixerEnabled(bool Enabled,int64 Epoch,int64& Revision,FString& Reason)
{
 if(!ValidateControl(Epoch,Reason))return false;
 MixerEnabled=ControlTargets.MixerEnabled=Enabled;Revision=++ControlTargets.Revision;PublishControls();return true;
}
bool APowderPlantTest::SetMixerRPM(double RPM,int64 Epoch,int64& Revision,FString& Reason)
{
 if(!ValidateControl(Epoch,Reason))return false;
 if(!FMath::IsFinite(RPM)||RPM<PlantControl::MinRPM||RPM>PlantControl::MaxRPM){Reason=TEXT("목표 회전속도는 유한한 0~30 rpm이어야 합니다.");return false;}
 MixerRPM=ControlTargets.RPM=RPM;Revision=++ControlTargets.Revision;PublishControls();return true;
}
bool APowderPlantTest::SetSimulationRunning(bool Run,int64 Epoch,int64& Revision,FString& Reason)
{
 if(!ValidateControl(Epoch,Reason))return false;
 Running=ControlTargets.Running=Run;Revision=++ControlTargets.Revision;PublishControls();return true;
}
bool APowderPlantTest::RequestAllClosed(int64 Epoch,int64& Revision,FString& Reason)
{
 if(!ValidateControl(Epoch,Reason))return false;
 for(int32 I=0;I<SupplierOpen.Num();++I)SupplierOpen[I]=false;
 ControlTargets.Suppliers=0;WeighingHopperOpen=ControlTargets.WeighingOpen=false;
 ReceiverOpen=ControlTargets.ReceiverOpen=false;MixerEnabled=ControlTargets.MixerEnabled=false;
 // One atomic mailbox update: no running batch can see the old opening/rotation targets with this resume.
 Running=ControlTargets.Running=true;Revision=++ControlTargets.Revision;PublishControls();return true;
}
