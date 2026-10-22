#include "PowderProcessTwin.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "DrawDebugHelpers.h"
#include "UnrealClient.h"
#include <sstream>
#include <iomanip>
APowderProcessTwin::APowderProcessTwin(){
 PrimaryActorTick.bCanEverTick=true;RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 auto make=[&](const TCHAR* Name,UStaticMesh* Mesh){auto* C=CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);C->SetupAttachment(RootComponent);C->SetStaticMesh(Mesh);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);return C;};
 PowderA=make(TEXT("IngredientA"),Sphere.Object);PowderB=make(TEXT("IngredientB"),Sphere.Object);PowderC=make(TEXT("IngredientC"),Sphere.Object);Structure=make(TEXT("CutawayChannels"),Cube.Object);
}
FVector APowderProcessTwin::Position(std::size_t Node)const{
 if(Node<3)return FVector(-400,(double(Node)-1)*160,220);
 if(Node==Model->weigh)return FVector(-180,0,160);
 if(Node>=Model->pipe&&Node<Model->receiver){double t=(double(Node-Model->pipe)+.5)/Model->config.pipe_cells;return FVector(-80+200*t,0,160);}
 if(Node==Model->receiver)return FVector(180,0,160);
 unsigned I=static_cast<unsigned>(Node-Model->mixer);return FVector(420+(I%4)*30,(int((I/4)%3)-1)*35,(I/12)*25);
}
void APowderProcessTwin::BuildGeometry(){Structure->ClearInstances();auto cube=[&](FVector P,FVector Scale,FQuat Q=FQuat::Identity){Structure->AddInstance(FTransform(Q,P,Scale));};
 auto channel=[&](FVector A,FVector B){FVector X=(B-A).GetSafeNormal(),Y=FVector::CrossProduct(FVector::UpVector,X).GetSafeNormal(),Z=FVector::CrossProduct(X,Y);FQuat Q=FRotationMatrix::MakeFromXY(X,Y).ToQuat();double L=(B-A).Length();FVector Mid=(A+B)*.5;cube(Mid-Z*7,FVector(L/100,.16,.02),Q);cube(Mid-Y*8,FVector(L/100,.02,.14),Q);cube(Mid+Y*8,FVector(L/100,.02,.14),Q);};
 for(unsigned I=0;I<3;++I){cube(Position(I)-FVector(0,0,30),FVector(.7,.7,.02));FVector D=(Position(Model->weigh)-Position(I)).GetSafeNormal();channel(Position(I)+D*45,Position(Model->weigh)-D*45);}
 cube(Position(Model->weigh)-FVector(0,0,30),FVector(.7,.7,.02));channel(Position(Model->weigh)+FVector(40,0,0),Position(Model->receiver)-FVector(40,0,0));
 // Receiver is a cutaway wire envelope, with an unobstructed sloping discharge.
 FVector DumpDirection=(FVector(385,0,25)-Position(Model->receiver)).GetSafeNormal();channel(Position(Model->receiver)+DumpDirection*45,FVector(385,0,25));cube(FVector(465,0,-15),FVector(1.5,1.2,.02));
}
void APowderProcessTwin::ResetSimulation(){try{powder::BulkProcessConfig C;C.total_kg=TotalKg;C.rpm=RPM;C.bulk_density=BulkDensity;C.particle_tau=DragRelaxationSeconds;C.mix_exchange=MixingExchange;C.dead_zone=DeadZoneFactor;C.pump_target_pa=VacuumTargetPa;C.feed_kg_s=SupplyKgPerSecond;C.reserve_factor=RawReserveFactor;Model=std::make_unique<powder::BulkProcess>(C);Debt=0;ElapsedWall=0;WindowWall=0;WindowPhysical=0;MinWindowRate=1e30;LastWall=FPlatformTime::Seconds();NextSave=0;Frames=0;Rows=0;ResultSaved=false;TransferShot=false;MixShot=false;EndWall=0;PhysicalSeconds=0;Error.Empty();
 auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/M_BulkFlow.M_BulkFlow"));UInstancedStaticMeshComponent* Components[]={PowderA,PowderB,PowderC,Structure};FLinearColor Colors[]={FLinearColor(1.f,.03f,.01f),FLinearColor(.03f,1.f,.15f),FLinearColor(1.f,.65f,.01f),FLinearColor(.04f,.07f,.1f)};for(int32 I=0;I<4;++I){if(Base)Components[I]->SetMaterial(0,Base);if(auto* M=Components[I]->CreateDynamicMaterialInstance(0))M->SetVectorParameterValue(TEXT("Tint"),Colors[I]);}
 if(HasActorBegunPlay()){int32 Run=0;FParse::Value(FCommandLine::Get(),TEXT("ProcessRun="),Run);OutputDir=FPaths::ProjectSavedDir()/(Benchmark?FString::Printf(TEXT("Optimization36_%d_r%d"),FMath::RoundToInt(TotalKg),Run):TEXT("ProcessTwin_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));IFileManager::Get().MakeDirectory(*OutputDir,true);if(!FFileHelper::SaveStringToFile(TEXT("physical_s,phase,ingredient,sensor_kg,pressure_pa,mixer_kg,mix_index,total_a,total_b,total_c,real_wall_s\n"),*(OutputDir/TEXT("trace.csv"))))throw std::runtime_error("cannot initialize trace");}
 BuildGeometry();Refresh();}catch(const std::exception& E){Error=UTF8_TO_TCHAR(E.what());Running=false;}}
void APowderProcessTwin::OnConstruction(const FTransform& T){Super::OnConstruction(T);ResetSimulation();}
void APowderProcessTwin::BeginPlay(){Super::BeginPlay();Benchmark=FParse::Param(FCommandLine::Get(),TEXT("ProcessBenchmark"));if(Benchmark){FParse::Value(FCommandLine::Get(),TEXT("ProcessKg="),TotalKg);TargetRate=2;Running=true;}ResetSimulation();auto* Camera=GetWorld()->SpawnActor<ACameraActor>();Camera->SetActorLocation(GetActorTransform().TransformPosition(FVector(50,-900,750)));Camera->SetActorRotation((GetActorTransform().TransformPosition(FVector(50,0,100))-Camera->GetActorLocation()).Rotation());if(auto* PC=UGameplayStatics::GetPlayerController(this,0))PC->SetViewTarget(Camera);}
int32 APowderProcessTwin::InstanceCount()const{return PowderA->GetInstanceCount()+PowderB->GetInstanceCount()+PowderC->GetInstanceCount();}
void APowderProcessTwin::Refresh(){if(!Model)return;TArray<FTransform> Instances[3];int32 Budget=FMath::Clamp(DisplayBudget,0,5000);
 for(std::size_t N=0;N<Model->graph.nodes.size();++N){bool Pipe=N>=Model->pipe&&N<Model->receiver;bool Mix=N>=Model->mixer;FVector Centre=Position(N);for(unsigned G=0;G<3;++G){int32 Count=FMath::FloorToInt(Model->graph.nodes[N].mass[G]/TotalKg*Budget);for(int32 J=0;J<Count;++J){double U=double((J*37+G*13)%997)/997,V=double((J*67+G*31)%991)/991,Z=double((J*97+G*17)%983)/983;FVector P;
  if(Pipe){double Flow=Model->pipe_travel[N-Model->pipe];double Offset=FMath::Fmod(U+Flow/(Model->config.pipe_length_m/Model->config.pipe_cells),1.);P=Centre+FVector((Offset-.5)*(200./Model->config.pipe_cells), (V-.5)*7,(Z-.5)*7);}
  else if(Mix){double Angle=(U+Model->mix_turns)*2*PI;P=Centre+FVector(FMath::Cos(Angle)*8*V,FMath::Sin(Angle)*8*V,(Z-.5)*16);}
  else P=Centre+FVector((U-.5)*50,(V-.5)*50,(Z-.5)*40);
  Instances[G].Add(FTransform(FQuat::Identity,P,FVector(.035)));}}}
 // Through-flow markers are massless and restricted to supported connection interiors.
 for(const auto& F:Model->last_flows){if(F.actual<=1e-10||(F.from>=Model->pipe&&F.from+1<Model->receiver)||F.to>=Model->mixer)continue;FVector A=Position(F.from),B=Position(F.to);for(unsigned G=0;G<3;++G){double Mass=powder::bulk_sum(Model->graph.nodes[F.from].mass);if(Mass<=0||Model->graph.nodes[F.from].mass[G]/Mass<.05)continue;for(int32 J=0;J<8;++J){double T=FMath::Fmod(J/8.+Model->time,1.);Instances[G].Add(FTransform(FQuat::Identity,FMath::Lerp(A,B,T),FVector(.035)));}}}
 // Discharge is aggregated across the eight destination cells and follows its cutaway channel.
 bool Discharging=false;for(const auto& F:Model->last_flows)if(F.from==Model->receiver&&F.actual>1e-10)Discharging=true;
 if(Model->phase==powder::BulkPhase::Dump&&DumpValveOpen&&Discharging){double M=powder::bulk_sum(Model->graph.nodes[Model->receiver].mass);if(M>1e-8)for(int32 J=0;J<16;++J){double T=FMath::Fmod(J/16.+Model->time,1.);Instances[Model->ingredient].Add(FTransform(FQuat::Identity,FMath::Lerp(Position(Model->receiver),FVector(385,0,25),T),FVector(.035)));}}
 UInstancedStaticMeshComponent* Components[]={PowderA,PowderB,PowderC};for(int32 I=0;I<3;++I){if(Components[I]->GetInstanceCount()!=Instances[I].Num()){Components[I]->ClearInstances();Components[I]->AddInstances(Instances[I],false,false);}else if(Instances[I].Num())Components[I]->BatchUpdateInstancesTransforms(0,Instances[I],false,true,true);}
 PhysicalSeconds=Model->time;Phase=UTF8_TO_TCHAR(powder::bulk_phase_name(Model->phase));WeighedKg=Model->sensor_kg;ReceiverPressurePa=Model->pressure_pa;MixerKg=powder::bulk_sum(Model->mixer_mass());MixingIndex=Model->mixing_index();}
void APowderProcessTwin::SaveRecord(){if(OutputDir.IsEmpty())return;auto M=Model->graph.totals();FString Row=FString::Printf(TEXT("%.9f,%s,%u,%.9f,%.9f,%.9f,%.9f,%.12f,%.12f,%.12f,%.9f\n"),Model->time,UTF8_TO_TCHAR(powder::bulk_phase_name(Model->phase)),Model->ingredient,Model->sensor_kg,Model->pressure_pa,powder::bulk_sum(Model->mixer_mass()),Model->mixing_index(),M[0],M[1],M[2],ElapsedWall);
 if(!FFileHelper::SaveStringToFile(Row,*(OutputDir/TEXT("trace.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append))throw std::runtime_error("trace write failed");++Rows;
 std::ostringstream S;S<<std::setprecision(17)<<"{\"schema\":1,\"time_s\":"<<Model->time<<",\"phase\":\""<<powder::bulk_phase_name(Model->phase)<<"\",\"pressure_pa\":"<<Model->pressure_pa<<",\"nodes\":[";for(std::size_t I=0;I<Model->graph.nodes.size();++I){if(I)S<<',';auto A=Model->graph.nodes[I].mass;S<<'['<<A[0]<<','<<A[1]<<','<<A[2]<<']';}S<<"],\"restart_supported\":false}";
 if(!FFileHelper::SaveStringToFile(UTF8_TO_TCHAR(S.str().c_str()),*(OutputDir/TEXT("state.json"))))throw std::runtime_error("state write failed");}
void APowderProcessTwin::SaveResult(){if(ResultSaved||OutputDir.IsEmpty())return;auto M=Model->graph.totals();double ErrorKg=0;for(unsigned G=0;G<3;++G)ErrorKg=FMath::Max(ErrorKg,FMath::Abs(M[G]-Model->initial[G]));FString Json=FString::Printf(TEXT("{\"complete\":%s,\"kg\":%.9f,\"physical_s\":%.9f,\"real_wall_s\":%.9f,\"rate\":%.9f,\"minimum_one_second_window_rate\":%.9f,\"frames\":%d,\"trace_rows\":%d,\"mass_error_kg\":%.12g,\"mixer_kg\":%.9f,\"mix_index\":%.9f,\"render_and_save_included\":true,\"physical_accuracy_validated\":false}"),Complete()?TEXT("true"):TEXT("false"),TotalKg,Model->time,ElapsedWall,MeasuredRate,MinWindowRate,Frames,Rows,ErrorKg,powder::bulk_sum(Model->mixer_mass()),Model->mixing_index());if(!FFileHelper::SaveStringToFile(Json,*(OutputDir/TEXT("result.json"))))throw std::runtime_error("result write failed");ResultSaved=true;}
void APowderProcessTwin::Tick(float DeltaSeconds){Super::Tick(DeltaSeconds);if(!Model)return;double Now=FPlatformTime::Seconds(),Wall=Now-LastWall;LastWall=Now;
 try{if(Running&&Error.IsEmpty()&&!Complete()){
  ElapsedWall+=Wall;WindowWall+=Wall;Debt+=Wall*FMath::Clamp(TargetRate,.1,10.);Model->controls={SupplyEnabled,TransferValveOpen,PumpEnabled,DumpValveOpen,RPM};double Start=FPlatformTime::Seconds();while(Debt>=.005&&FPlatformTime::Seconds()-Start<.004&&!Complete()){Model->advance(.005);Debt-=.005;}Refresh();++Frames;MeasuredRate=ElapsedWall>0?Model->time/ElapsedWall:0;
  if(WindowWall>=1){MinWindowRate=FMath::Min(MinWindowRate,(Model->time-WindowPhysical)/WindowWall);WindowPhysical=Model->time;WindowWall=0;}
  if(Model->time>=NextSave){SaveRecord();NextSave=Model->time+1;}
  if(Complete()){SaveRecord();SaveResult();EndWall=Now;Running=false;}
 }
#if WITH_DEV_AUTOMATION_TESTS
 if(Benchmark){if(Model->phase==powder::BulkPhase::Transfer&&Model->phase_time>.5&&!TransferShot){FScreenshotRequest::RequestScreenshot(OutputDir/TEXT("transfer.png"),true,false);TransferShot=true;}if(Model->phase==powder::BulkPhase::Mix&&Model->mix_time>10&&!MixShot){FScreenshotRequest::RequestScreenshot(OutputDir/TEXT("mix.png"),true,false);MixShot=true;}if(ResultSaved&&Now-EndWall>1)FPlatformMisc::RequestExit(false);if(ElapsedWall>180){Error=TEXT("Benchmark timeout");Running=false;FPlatformMisc::RequestExitWithStatus(false,1);}}
#endif
 }catch(const std::exception& E){Error=UTF8_TO_TCHAR(E.what());Running=false;}
 if(GEngine)GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this),0,FColor::Cyan,FString::Printf(TEXT("VIRTUAL bulk process | %s | t %.2fs %.3fx | weigh %.3fkg P %.0fPa | mixer %.3fkg I %.3f | RPM %.1f | %s"),*Phase,PhysicalSeconds,MeasuredRate,WeighedKg,ReceiverPressurePa,MixerKg,MixingIndex,RPM,*Error));
 auto label=[&](FVector P,const FString& S){DrawDebugString(GetWorld(),GetActorTransform().TransformPosition(P),S,nullptr,FColor::White,0,true);};for(unsigned I=0;I<3;++I)label(Position(I)+FVector(0,0,55),FString::Printf(TEXT("Raw %c"),'A'+I));label(Position(Model->weigh)+FVector(0,0,55),TEXT("Weigh"));label(Position(Model->receiver)+FVector(0,0,55),TEXT("Vacuum receiver"));label(FVector(465,0,85),TEXT("Mixer - reduced model"));
 for(std::size_t N:{std::size_t(0),std::size_t(1),std::size_t(2),Model->weigh,Model->receiver})DrawDebugBox(GetWorld(),GetActorTransform().TransformPosition(Position(N)),FVector(35,35,30),GetActorQuat(),FColor::Silver,false,0,0,1);
}
