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
 std::thread Thread;std::mutex Mutex;std::atomic<bool> Quit{false},Run{true};
 std::atomic<double> Rate{.1},Drop{1000},RPM{15};std::atomic<uint32> RequestedSuppliers{0};std::atomic<bool> RequestedHopper{false},RequestedReceiver{false};
 std::vector<powder::Grain> Points;std::string Error;double Time=0,Wall=0,Angle=0;
 int Phase=0,WH=0,RH=0,MX=0;bool HopperOpen=false,ReceiverOpen=false;uint32 OpenSuppliers=0;uint64 Generation=1;
 FPlantRuntime(powder::GranularConfig C,std::vector<powder::Grain> P,std::vector<powder::PlantRoute> Routes,double Timeout,double Fraction,double TransferWait,bool Start,double RequestedRate,double Pressure,double Speed):Points(P){
  Run=Start;Rate=RequestedRate;Drop=Pressure;RPM=Speed;
  Thread=std::thread([this,C=std::move(C),P=std::move(P),Routes=std::move(Routes),Timeout,Fraction,TransferWait]()mutable{
   try{
    double AppliedPressure=Drop;auto Gates=powder::plant_supplier_gates(C.plant_data);
    int HopperGate=powder::plant_weighing_gate(C.plant_data),ReceiverGate=powder::plant_receiver_gate(C.plant_data);bool OpenHopper=false,PreviousHopper=false,OpenReceiver=false,PreviousReceiver=false;
    uint32 Open=0,PreviousRequest=0,All=Gates.size()==32?~uint32(0):(uint32(1)<<Gates.size())-1;
    powder::plant_set_manual_valves(C.plant_data,Routes,AppliedPressure,0,0,false,false);
    powder::plant_set_receiver_valve(C.plant_data,false);
    powder::GranularGPU GPU(C,P,std::vector<powder::Grain>(P.size()));
    int PhaseNow=0;double StageStart=0,Active=0;
    auto LastPublish=std::chrono::steady_clock::now(),LastControl=LastPublish;
    while(!Quit){
     if(!Run){std::this_thread::sleep_for(std::chrono::milliseconds(5));continue;}
     auto Begin=std::chrono::steady_clock::now();double R=Rate,S=RPM,D=Drop;
     if(!std::isfinite(R)||R<=0||R>.1||!std::isfinite(S)||S<0||S>30)throw std::runtime_error("invalid live rate/RPM");
     bool WantHopper=RequestedHopper.load(),WantReceiver=RequestedReceiver.load();
     uint32 Request=RequestedSuppliers.load()&All;
     if(D!=AppliedPressure||Request!=PreviousRequest||WantHopper!=PreviousHopper||WantReceiver!=PreviousReceiver||((Open!=Request||OpenHopper!=WantHopper||OpenReceiver!=WantReceiver)&&std::chrono::duration<double>(Begin-LastControl).count()>.1)){
      uint32 NextOpen=Open|Request,Closing=Open&~Request;bool NextHopper=OpenHopper||WantHopper,NextReceiver=OpenReceiver||WantReceiver;
      if(Closing||(OpenHopper&&!WantHopper)||(OpenReceiver&&!WantReceiver)){auto Current=GPU.snapshot();if(OpenReceiver&&!WantReceiver&&powder::plant_supplier_closure_clear(C.plant_data[ReceiverGate],Current,C.radius))NextReceiver=false;if(OpenHopper&&!WantHopper&&powder::plant_supplier_closure_clear(C.plant_data[HopperGate],Current,C.radius))NextHopper=false;for(size_t i=0;i<Gates.size();++i)if((Closing&(uint32(1)<<i))&&powder::plant_supplier_closure_clear(C.plant_data[Gates[i]],Current,C.radius))NextOpen&=~(uint32(1)<<i);}
      if(NextOpen!=Open||Request!=PreviousRequest||D!=AppliedPressure||NextHopper!=OpenHopper||WantHopper!=PreviousHopper||NextReceiver!=OpenReceiver||WantReceiver!=PreviousReceiver){
       int NextPhase=NextHopper?1:0;if(NextPhase!=PhaseNow){PhaseNow=NextPhase;StageStart=GPU.time();C.plant_data[4].x=float(PhaseNow);}
       powder::plant_set_manual_valves(C.plant_data,Routes,D,NextOpen,Request,NextHopper,WantHopper);powder::plant_set_receiver_valve(C.plant_data,NextReceiver);GPU.update_plant_field(C.plant_data);
       OpenReceiver=NextReceiver;PreviousReceiver=WantReceiver;Open=NextOpen;OpenHopper=NextHopper;PreviousHopper=WantHopper;PreviousRequest=Request;AppliedPressure=D;
      }LastControl=Begin;
     }
     GPU.advance(16,float(S),false);
     double Compute=std::chrono::duration<double>(std::chrono::steady_clock::now()-Begin).count(),Sleep=16*C.dt/R-Compute;
     if(Sleep>0)std::this_thread::sleep_for(std::chrono::duration<double>(Sleep));
     Active+=std::chrono::duration<double>(std::chrono::steady_clock::now()-Begin).count();
     if(PhaseNow==0||(PhaseNow==1&&!WantHopper))StageStart=GPU.time(); // An intentional closed supply is not a timeout.
     if(std::chrono::duration<double>(std::chrono::steady_clock::now()-LastPublish).count()>.1){
      auto Next=GPU.snapshot();int W,H,M;Count(Next,W,H,M);
      {std::lock_guard<std::mutex>L(Mutex);Points=std::move(Next);Time=GPU.time();Angle=GPU.angle();Wall=Active;WH=W;RH=H;MX=M;Phase=PhaseNow;OpenSuppliers=Open;HopperOpen=OpenHopper;ReceiverOpen=OpenReceiver;++Generation;}
      // Manual valve operation may intentionally retain material indefinitely.
      LastPublish=std::chrono::steady_clock::now();
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
void APowderPlantTest::ResetSimulation(){Stop();Error.Empty();PhysicalSeconds=ActualRate=0;Generation=0;CurrentPhase=0;AppliedSupplierMask=0;WeighingHopperOpen=AppliedHopperOpen=false;ReceiverOpen=AppliedReceiverOpen=MixerEnabled=false;InWeighingHopper=InReceiver=InMixer=0;try{
  InitializeSuppliers();for(int i=0;i<SupplierOpen.Num();++i)SupplierOpen[i]=false;
  if(!std::isfinite(FixedDt)||FixedDt<1e-6||FixedDt>2.5e-5||ContactIterations<4||ContactIterations>32||!std::isfinite(StageTimeoutSeconds)||StageTimeoutSeconds<=0)throw std::runtime_error("invalid plant test controls");
  powder::plant_discharge_ready(0,1,0,MinimumReceiverFraction,ReceiverTransferSeconds);
  FString Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/DataDirectory);auto P=ReadCharge(Root/TEXT("test_charge.csv"));ParticleCount=int32(P.size());int Types=0;for(auto A:P)Types=FMath::Max(Types,int(A.w)+1);
  while(Groups.Num()<Types){auto*G=NewObject<UInstancedStaticMeshComponent>(this);G->SetupAttachment(RootComponent);G->SetCollisionEnabled(ECollisionEnabled::NoCollision);G->SetCastShadow(false);G->RegisterComponent();AddInstanceComponent(G);Groups.Add(G);}
  auto*Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/DEM/SM_GrainSphere.SM_GrainSphere"));if(!Mesh)Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));auto*Mat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/M_BulkFlow.M_BulkFlow"));FLinearColor Colors[3]{FLinearColor(.9,.025,.01),FLinearColor(.01,.2,.9),FLinearColor(.9,.7,.01)};
  for(int I=0;I<Groups.Num();++I){auto*G=Groups[I].Get();G->ClearInstances();G->SetStaticMesh(Mesh);if(Mat){G->SetMaterial(0,Mat);if(auto*M=G->CreateDynamicMaterialInstance(0))M->SetVectorParameterValue(TEXT("Tint"),Colors[I%3]);}}
  double Diameter=Mesh->GetBounds().BoxExtent.GetMax()*2;for(auto A:P)Groups[int(A.w)]->AddInstance(FTransform(FQuat::Identity,FVector(A.x,-A.y,A.z)*100,FVector(.25/Diameter)));
  Stage=TEXT("Initial charge: 10000 per supplier");if(HasActorBegunPlay()){powder::GranularConfig C;C.plant=true;C.plant_threaded=true;C.kernel_optimize=true;C.plant_data=powder::read_plant_data(TCHAR_TO_UTF8(*(Root/TEXT("runtime.plant"))));C.radius=.00125f;C.count=unsigned(P.size());C.dt=float(FixedDt);C.iterations=unsigned(ContactIterations);auto Routes=powder::read_plant_routes(TCHAR_TO_UTF8(*(Root/TEXT("runtime.routes"))));for(auto V:Rotor)if(V.Actor)V.Actor->SetActorTransform(V.Initial);Runtime=new FPlantRuntime(std::move(C),std::move(P),std::move(Routes),StageTimeoutSeconds,MinimumReceiverFraction,ReceiverTransferSeconds,Running,TargetRate,TestPressureDropPa,0);}
 }catch(const std::exception&E){Error=UTF8_TO_TCHAR(E.what());Running=false;}}
void APowderPlantTest::BeginPlay(){Super::BeginPlay();if(GEngine)GEngine->RemoveOnScreenDebugMessage(3101);if(auto*FPS=IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))){PreviousMaxFPS=FPS->GetFloat();FPS->Set(30.f,ECVF_SetByCode);}FParse::Value(FCommandLine::Get(),TEXT("PlantSmokeSeconds="),SmokeSeconds);FParse::Value(FCommandLine::Get(),TEXT("PlantWallLimit="),SmokeWallLimit);SmokeStart=FPlatformTime::Seconds();CacheRotor();ValveVisuals.Empty();for(TActorIterator<AActor>It(GetWorld());It;++It){if(auto*C=Cast<APlantVisibilityController>(*It))Display=C;const FString N=It->GetActorNameOrLabel();if((N.StartsWith(TEXT("V101"))&&N.EndsWith(TEXT("_Disc")))||N==TEXT("ASSUMED_VT_MovingGate")||N==TEXT("BV272_210_Disc")||N==TEXT("BV272_212_Disc_hub")||N==TEXT("BV122_210_Disc")||N==TEXT("BV122_212_Disc_hub"))ValveVisuals.Add(*It);}ResetSimulation();}
void APowderPlantTest::Show(){if(!Runtime)return;std::lock_guard<std::mutex>L(Runtime->Mutex);if(!Runtime->Error.empty()){Error=UTF8_TO_TCHAR(Runtime->Error.c_str());Running=false;}if(Generation==Runtime->Generation)return;Generation=Runtime->Generation;PhysicalSeconds=Runtime->Time;ActualRate=Runtime->Wall>0?PhysicalSeconds/Runtime->Wall:0;InWeighingHopper=Runtime->WH;InReceiver=Runtime->RH;InMixer=Runtime->MX;CurrentPhase=Runtime->Phase;AppliedSupplierMask=Runtime->OpenSuppliers;AppliedHopperOpen=Runtime->HopperOpen;AppliedReceiverOpen=Runtime->ReceiverOpen;Stage=Runtime->Phase==0?TEXT("공급기 → 칭량 호퍼"):Runtime->Phase==1?TEXT("칭량 호퍼 → 수신기"):TEXT("수신기 → 혼합기");
 TArray<TArray<FTransform>> T;T.SetNum(Groups.Num());for(auto A:Runtime->Points){auto*G=Groups[int(A.w)].Get();double Diameter=G->GetStaticMesh()->GetBounds().BoxExtent.GetMax()*2;T[int(A.w)].Add(FTransform(FQuat::Identity,FVector(A.x,-A.y,A.z)*100,FVector(.25/Diameter)));}for(int I=0;I<Groups.Num();++I)Groups[I]->BatchUpdateInstancesTransforms(0,T[I],false,true,true);
 for(auto V:ValveVisuals)if(V){const FString N=V->GetActorNameOrLabel();const int SupplierIndex=N.Len()>4?int(N[4]-TCHAR('A')):-1;bool Open=N.StartsWith(TEXT("BV122"))||(N.StartsWith(TEXT("V101"))&&SupplierIndex>=0&&SupplierIndex<SupplierOpen.Num()&&Runtime->Phase<2&&((Runtime->OpenSuppliers&(uint32(1)<<SupplierIndex))!=0))||(N.StartsWith(TEXT("ASSUMED"))&&Runtime->HopperOpen&&Runtime->Phase<2)||(N.StartsWith(TEXT("BV272"))&&Runtime->ReceiverOpen);V->SetActorHiddenInGame(Open||(Display&&Display->ViewMode==EPlantViewMode::Hidden));}
 FQuat Q(FVector(1,0,0),-Runtime->Angle);FVector Pivot(300,0,90);for(auto V:Rotor)if(V.Actor){FTransform X=V.Initial;X.SetLocation(Pivot+Q.RotateVector(V.Initial.GetLocation()-Pivot));X.SetRotation(Q*V.Initial.GetRotation());V.Actor->SetActorTransform(X);}}
void APowderPlantTest::Tick(float D){Super::Tick(D);if(auto*PC=GetWorld()->GetFirstPlayerController()){if(PC->WasInputKeyJustPressed(EKeys::SpaceBar))Running=!Running;if(PC->WasInputKeyJustPressed(EKeys::R))ResetSimulation();if(PC->WasInputKeyJustPressed(EKeys::O))ExportSnapshot();if(auto*Camera=Cast<ACameraActor>(PC->GetViewTarget())){int View=-1;if(PC->WasInputKeyJustPressed(EKeys::One))View=0;if(PC->WasInputKeyJustPressed(EKeys::Two))View=1;if(PC->WasInputKeyJustPressed(EKeys::Three))View=2;if(PC->WasInputKeyJustPressed(EKeys::Four))View=3;if(View>=0){FVector Targets[]{FVector(0,-140,260),FVector(0,0,115),FVector(300,0,275),FVector(300,0,90)};FVector Target=Targets[View];Camera->SetActorLocation(Target+FVector(80,View==0?300:150,60));Camera->SetActorRotation((Target-Camera->GetActorLocation()).Rotation());}}}if(Runtime){Runtime->Run=Running;Runtime->Rate=TargetRate;Runtime->Drop=TestPressureDropPa;Runtime->RPM=MixerEnabled?MixerRPM:0;uint32 Mask=0;for(int i=0;i<SupplierOpen.Num();++i)if(SupplierOpen[i])Mask|=uint32(1)<<i;Runtime->RequestedSuppliers=Mask;Runtime->RequestedHopper=WeighingHopperOpen;Runtime->RequestedReceiver=ReceiverOpen;}LastShow+=D;if(LastShow>.1){Show();LastShow=0;}if(SmokeSeconds>0){if(!SmokeShot&&PhysicalSeconds>=SmokeSeconds*.4){if(auto*PC=GetWorld()->GetFirstPlayerController())if(auto*Camera=Cast<ACameraActor>(PC->GetViewTarget())){FVector Target(0,-140,260);Camera->SetActorLocation(Target+FVector(30,300,60));Camera->SetActorRotation((Target-Camera->GetActorLocation()).Rotation());}FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("PlantFlowPreview.png"),true,false);SmokeShot=true;}
 if(PhysicalSeconds>=SmokeSeconds||!Error.IsEmpty()||FPlatformTime::Seconds()-SmokeStart>SmokeWallLimit){ExportSnapshot();FString Report=FString::Printf(TEXT("{\"count\":%d,\"physical_s\":%.9g,\"actual_rate\":%.9g,\"wh\":%d,\"rh\":%d,\"mixer\":%d,\"failed\":%s}"),ParticleCount,PhysicalSeconds,ActualRate,InWeighingHopper,InReceiver,InMixer,Error.IsEmpty()&&PhysicalSeconds>=SmokeSeconds?TEXT("false"):TEXT("true"));FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("PlantSmoke.json")));if(!Error.IsEmpty())FFileHelper::SaveStringToFile(Error,*(FPaths::ProjectSavedDir()/TEXT("PlantSmokeError.txt")));FPlatformMisc::RequestExitWithStatus(false,Error.IsEmpty()&&PhysicalSeconds>=SmokeSeconds?0:1);SmokeSeconds=0;}}}
void APowderPlantTest::ExportSnapshot(){if(!Runtime)return;std::lock_guard<std::mutex>L(Runtime->Mutex);std::ostringstream S;S<<"id,species,x_m,y_m,z_m\n";unsigned Id=0;for(auto A:Runtime->Points)S<<Id++<<','<<A.w<<','<<A.x<<','<<A.y<<','<<A.z<<'\n';FFileHelper::SaveStringToFile(UTF8_TO_TCHAR(S.str().c_str()),*(FPaths::ProjectSavedDir()/TEXT("PlantSnapshot.csv")));}
void APowderPlantTest::EndPlay(const EEndPlayReason::Type Reason){Stop();if(auto*FPS=IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))if(FPS->GetFloat()==30.f)FPS->Set(PreviousMaxFPS,ECVF_SetByCode);Super::EndPlay(Reason);}

void APowderPlantTest::InitializeSuppliers(){
 FString Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/DataDirectory);
 auto Data=powder::read_plant_data(TCHAR_TO_UTF8(*(Root/TEXT("runtime.plant"))));
 SupplierOpen.Init(false,int32(powder::plant_supplier_gates(Data).size()));
}
void APowderPlantTest::ToggleSupplier(int32 Index){
 if(HasActorBegunPlay()&&CurrentPhase<2&&Error.IsEmpty()&&SupplierOpen.IsValidIndex(Index))SupplierOpen[Index]=!SupplierOpen[Index];
}
void APowderPlantTest::ToggleWeighingHopper(){
 if(HasActorBegunPlay()&&CurrentPhase<2&&Error.IsEmpty())WeighingHopperOpen=!WeighingHopperOpen;
}
void APowderPlantTest::ToggleReceiver(){if(HasActorBegunPlay()&&Error.IsEmpty())ReceiverOpen=!ReceiverOpen;}
void APowderPlantTest::ToggleMixer(){if(HasActorBegunPlay()&&Error.IsEmpty())MixerEnabled=!MixerEnabled;}
TSharedRef<SWidget> APowderPlantTest::BuildSimulationPanel(){
 if(SupplierOpen.IsEmpty()){try{InitializeSuppliers();}catch(const std::exception&E){Error=UTF8_TO_TCHAR(E.what());}}
 const TWeakObjectPtr<APowderPlantTest> Weak(this);
 TSharedRef<SVerticalBox> Panel=SNew(SVerticalBox);
 Panel->AddSlot().AutoHeight().Padding(0,0,0,6)[SNew(STextBlock).Text(FText::FromString(TEXT("시뮬레이션 정보")))];
 Panel->AddSlot().AutoHeight()[SNew(STextBlock).WrapTextAt(350).Text_Lambda([Weak](){
  if(!Weak.IsValid())return FText::GetEmpty();const auto*P=Weak.Get();
  return FText::FromString(FString::Printf(TEXT("시간 %.3f초 | 실제 속도 %.4f배\n입자 %d개 | %s\n칭량 %d · 수신기 %d · 혼합기 %d\n통로/기타 %d | %s"),P->PhysicalSeconds,P->ActualRate,P->ParticleCount,*P->Stage,P->InWeighingHopper,P->InReceiver,P->InMixer,P->ParticleCount-P->InWeighingHopper-P->InReceiver-P->InMixer,P->Running?TEXT("계산 중"):TEXT("일시정지")));
 })];
 Panel->AddSlot().AutoHeight().Padding(0,8,0,4)[SNew(STextBlock).Text(FText::FromString(TEXT("공급기 밸브 · 시작/초기화 시 모두 닫힘")))];
 for(int32 I=0;I<SupplierOpen.Num();++I){
  Panel->AddSlot().AutoHeight().Padding(0,2)[SNew(SHorizontalBox)
   + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text_Lambda([Weak,I](){
    if(!Weak.IsValid()||!Weak->SupplierOpen.IsValidIndex(I))return FText::GetEmpty();
    bool Want=Weak->SupplierOpen[I],Open=(Weak->AppliedSupplierMask&(uint32(1)<<I))!=0;
    const TCHAR*State=Want?(Open?TEXT("열림"):TEXT("열림 대기")):(Open?TEXT("닫힘 대기 (입자 통과 중)"):TEXT("닫힘"));
    return FText::FromString(FString::Printf(TEXT("공급기 %c · %s"),TCHAR('A'+I),State));
   })]
   + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).IsFocusable(false).ContentPadding(FMargin(14,5))
    .IsEnabled_Lambda([Weak](){return Weak.IsValid()&&Weak->HasActorBegunPlay()&&Weak->CurrentPhase<2&&Weak->Error.IsEmpty();})
    .OnClicked_Lambda([Weak,I](){if(Weak.IsValid())Weak->ToggleSupplier(I);return FReply::Handled();})
    [SNew(STextBlock).Text_Lambda([Weak,I](){return FText::FromString(Weak.IsValid()&&Weak->SupplierOpen.IsValidIndex(I)&&Weak->SupplierOpen[I]?TEXT("닫기"):TEXT("열기"));})]]];
 }
 Panel->AddSlot().AutoHeight().Padding(0,8,0,0)[SNew(SHorizontalBox)
  + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text_Lambda([Weak](){
   if(!Weak.IsValid())return FText::GetEmpty();bool Want=Weak->WeighingHopperOpen,Open=Weak->AppliedHopperOpen;
   const TCHAR*State=Want?(Open?TEXT("열림"):TEXT("열림 대기")):(Open?TEXT("닫힘 대기 (입자 통과 중)"):TEXT("닫힘"));
   return FText::FromString(FString::Printf(TEXT("공용 칭량 호퍼 · %s"),State));
  })]
  + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).IsFocusable(false).ContentPadding(FMargin(14,5))
   .IsEnabled_Lambda([Weak](){return Weak.IsValid()&&Weak->HasActorBegunPlay()&&Weak->CurrentPhase<2&&Weak->Error.IsEmpty();})
   .OnClicked_Lambda([Weak](){if(Weak.IsValid())Weak->ToggleWeighingHopper();return FReply::Handled();})
   [SNew(STextBlock).Text_Lambda([Weak](){return FText::FromString(Weak.IsValid()&&Weak->WeighingHopperOpen?TEXT("닫기"):TEXT("열기"));})]]];
 for(int Control=0;Control<2;++Control){
  Panel->AddSlot().AutoHeight().Padding(0,6,0,0)[SNew(SHorizontalBox)
   + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text_Lambda([Weak,Control](){
    if(!Weak.IsValid())return FText::GetEmpty();
    if(Control==1)return FText::FromString(FString::Printf(TEXT("혼합기 · %s (%.1f RPM)"),Weak->MixerEnabled?TEXT("가동 요청"):TEXT("정지"),Weak->MixerRPM));
    bool Want=Weak->ReceiverOpen,Open=Weak->AppliedReceiverOpen;
    return FText::FromString(FString::Printf(TEXT("수취부 → 혼합기 · %s"),Want?(Open?TEXT("열림"):TEXT("열림 대기")):(Open?TEXT("닫힘 대기"):TEXT("닫힘"))));
   })]
   + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).IsFocusable(false).ContentPadding(FMargin(14,5))
    .IsEnabled_Lambda([Weak](){return Weak.IsValid()&&Weak->HasActorBegunPlay()&&Weak->Error.IsEmpty();})
    .OnClicked_Lambda([Weak,Control](){if(Weak.IsValid()){if(Control)Weak->ToggleMixer();else Weak->ToggleReceiver();}return FReply::Handled();})
    [SNew(STextBlock).Text_Lambda([Weak,Control](){return FText::FromString(Control?(Weak.IsValid()&&Weak->MixerEnabled?TEXT("정지"):TEXT("시작")):(Weak.IsValid()&&Weak->ReceiverOpen?TEXT("닫기"):TEXT("열기")));})]]];
 }
 Panel->AddSlot().AutoHeight().Padding(0,8,0,0)[SNew(STextBlock).WrapTextAt(350).Text(FText::FromString(TEXT("1~4: 장치 확대 | Space: 일시정지\nR: 초기화 | O: 입자 저장\n관로 압력 근사 + GPU 입자 시험 · 미보정")))];
 Panel->AddSlot().AutoHeight().Padding(0,4)[SNew(STextBlock).WrapTextAt(350).ColorAndOpacity(FLinearColor(1,.25,.2)).Text_Lambda([Weak](){return Weak.IsValid()?FText::FromString(Weak->Error):FText::GetEmpty();})];
 return SNew(SBox).WidthOverride(360)[Panel];
}
