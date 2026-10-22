#include "PowderBulkPrototype.h"
#include "PowderCameraController.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "HAL/PlatformTime.h"
#include "DrawDebugHelpers.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UnrealClient.h"
#include "Materials/MaterialInstanceDynamic.h"
APowderBulkGameMode::APowderBulkGameMode(){PlayerControllerClass=APowderCameraController::StaticClass();DefaultPawnClass=nullptr;}
APowderBulkPrototype::APowderBulkPrototype(){
 PrimaryActorTick.bCanEverTick=true;RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 auto particles=[&](const TCHAR* Name){auto* C=CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);C->SetupAttachment(RootComponent);C->SetStaticMesh(Sphere.Object);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetMobility(EComponentMobility::Movable);return C;};
 GroupA=particles(TEXT("PowderA"));GroupB=particles(TEXT("PowderB"));GroupC=particles(TEXT("PowderC"));
 auto wall=[&](const TCHAR* Name,FVector Position,FVector Scale){auto* C=CreateDefaultSubobject<UStaticMeshComponent>(Name);C->SetupAttachment(RootComponent);C->SetStaticMesh(Cube.Object);C->SetRelativeLocation(Position);C->SetRelativeScale3D(Scale);return C;};
 wall(TEXT("Floor"),{250,0,-21},{5,.44,.02});wall(TEXT("LeftWall"),{250,-21,0},{5,.02,.4});wall(TEXT("RightWall"),{250,21,0},{5,.02,.4});wall(TEXT("Start"),{-1,0,0},{.02,.44,.44});wall(TEXT("End"),{501,0,0},{.02,.44,.44});Gate=wall(TEXT("Valve"),{200,0,0},{.02,.4,.4});
}
void APowderBulkPrototype::OnConstruction(const FTransform& Transform){Super::OnConstruction(Transform);ResetSimulation();}
void APowderBulkPrototype::ResetSimulation(){try{powder::BulkPrototype::Config C;C.mass=TotalKg;C.markers=static_cast<unsigned>(FMath::Clamp(DisplayCount,0,10000));C.gravity=-9.81*GetActorForwardVector().Z;C.normal_gravity=-9.81*GetActorUpVector().Z;Model=std::make_unique<powder::BulkPrototype>(C);Debt=0;RunWall=0;ObservedWall=0;LastTickWall=FPlatformTime::Seconds();PhysicalSeconds=0;ActualRate=0;Error.Empty();TArray<UStaticMeshComponent*> Meshes;GetComponents(Meshes);for(auto* M:Meshes){if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/M_BulkFlow.M_BulkFlow")))M->SetMaterial(0,Base);auto* Material=M->CreateDynamicMaterialInstance(0);if(Material){FLinearColor Color=M==GroupA?FLinearColor(1,.05,.01):M==GroupB?FLinearColor(.02,1,.1):M==GroupC?FLinearColor(1,.6,.01):FLinearColor(.08,.1,.14);Material->SetVectorParameterValue(TEXT("Tint"),Color);}}Refresh();}catch(const std::exception& E){Error=UTF8_TO_TCHAR(E.what());Running=false;}}
void APowderBulkPrototype::BeginPlay(){Super::BeginPlay();ResetSimulation();auto* Camera=GetWorld()->SpawnActor<ACameraActor>();Camera->SetActorLocation(GetActorTransform().TransformPosition({250,-250,400}));Camera->SetActorRotation((GetActorTransform().TransformPosition({250,0,0})-Camera->GetActorLocation()).Rotation());if(auto* PC=UGameplayStatics::GetPlayerController(this,0))PC->SetViewTarget(Camera);}
void APowderBulkPrototype::Refresh(){if(!Model)return;TArray<FTransform> A,B,C;for(const auto& P:Model->markers){FTransform T(FQuat::Identity,FVector(P.x*100,P.y*100,P.z*100),FVector(.03));(P.group==0?A:P.group==1?B:C).Add(T);}auto update=[](UInstancedStaticMeshComponent* M,const TArray<FTransform>& T){if(M->GetInstanceCount()!=T.Num()){M->ClearInstances();M->AddInstances(T,false,false);}else if(T.Num())M->BatchUpdateInstancesTransforms(0,T,false,true,true);};update(GroupA,A);update(GroupB,B);update(GroupC,C);Gate->SetVisibility(!ValveOpen);Gate->SetCollisionEnabled(ValveOpen?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryAndPhysics);}
void APowderBulkPrototype::Tick(float DeltaSeconds){Super::Tick(DeltaSeconds);if(!Model)return;const double Now=FPlatformTime::Seconds();const double WallDelta=Now-LastTickWall;LastTickWall=Now;
#if WITH_DEV_AUTOMATION_TESTS
 if(FParse::Param(FCommandLine::Get(),TEXT("BulkCapture"))){
  static bool Shot=false;if(RunWall>5&&!Shot){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Optimization28_visible.png"),true,false);Shot=true;}
  if(RunWall>8){FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"physical_s\":%.9f,\"real_wall_s\":%.9f,\"achieved_rate\":%.9f,\"scope\":\"8-second rendered smoke test, not final process acceptance\"}"),PhysicalSeconds,ObservedWall,ActualRate),*(FPaths::ProjectSavedDir()/TEXT("Optimization28_viewport.json")));FPlatformMisc::RequestExit(false);}
 }
#endif
 if(Running&&Error.IsEmpty()){ObservedWall+=WallDelta;RunWall+=DeltaSeconds;Debt+=DeltaSeconds*FMath::Clamp(TargetRate,.01,2.);double Begin=FPlatformTime::Seconds();try{Model->air=AirSpeed;Model->valve_open=ValveOpen;while(Debt>=.005&&FPlatformTime::Seconds()-Begin<.004){Model->advance(.005);Debt-=.005;}PhysicalSeconds=Model->time;ActualRate=ObservedWall>0?PhysicalSeconds/ObservedWall:0;Refresh();}catch(const std::exception& E){Error=UTF8_TO_TCHAR(E.what());Running=false;}}
 if(GEngine){auto M=Model->masses();GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this),0,FColor::Cyan,FString::Printf(TEXT("Bulk prototype | %.2f kg | t %.3f s | actual %.3fx | valve %s | air %.2f m/s | reduced flow, NOT individual DEM %s"),M[0]+M[1]+M[2],PhysicalSeconds,ActualRate,ValveOpen?TEXT("open"):TEXT("closed"),AirSpeed,*Error));}
 const TCHAR* Names[]={TEXT("Supply"),TEXT("Weigh placeholder"),TEXT("Transfer"),TEXT("Receiver placeholder"),TEXT("Mix placeholder")};for(int32 I=0;I<5;++I)DrawDebugString(GetWorld(),GetActorTransform().TransformPosition({I*100.+50,0,35}),Names[I],nullptr,FColor::White,0,true);
}
