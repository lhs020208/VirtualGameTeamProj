#include "PowderDEMTest.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/World.h"
#include "Engine/Engine.h"

#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "powder/geometry.hpp"
#include <sstream>
#include <map>
#include "HAL/FileManager.h"
#include "Misc/Guid.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDEMBridgeControls,"Powder.DEM.BackgroundBridge",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDEMBridgeControls::RunTest(const FString& Parameters)
{
 UWorld::InitializationValues IV;IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);
 if(!TestNotNull(TEXT("test world"),W))return false;
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* Floor=W->SpawnActor<AStaticMeshActor>();Floor->Tags.Add(TEXT("PowderRig"));
 Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
 Floor->SetActorLocation(FVector(0,0,-50));Floor->SetActorScale3D(FVector(4,4,1));
 W->InitializeActorsForPlay(FURL());
 auto* A=W->SpawnActor<APowderDEMTest>();A->DispatchBeginPlay();TestTrue(TEXT("actor lifecycle initialized"),A->IsActorInitialized()&&A->HasActorBegunPlay());
 auto Wait=[&](auto Predicate){const double End=FPlatformTime::Seconds()+15;while(FPlatformTime::Seconds()<End){A->Tick(1.f/60);if(Predicate())return true;FPlatformProcess::Sleep(.001f);}AddError(TEXT("worker acknowledgement timeout"));return false;};
 A->ParticleCount=1;A->DiameterM=.2;A->DensityKgM3=238.732414637843;A->FixedDt=2e-6;A->NormalDamping=800;A->TangentDamping=200;A->WallNormalDamping=800;A->WallTangentDamping=200;A->InitialPositionCm=FVector(0,0,10.000001);A->ResetSimulation();
 if(!Wait([&]{return A->IsReady();})){W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return false;}
 TestTrue(TEXT("configuration valid"),A->Error.IsEmpty());TestTrue(TEXT("Chaos disabled"),A->ChaosDisabled());
 A->StepSimulation();Wait([&]{return A->StepCount()==1;});TestFalse(TEXT("N pauses"),A->IsRunning());TestEqual(TEXT("no initial overlap history"),A->HistoryCount(),0);for(int I=0;I<32;++I)A->StepSimulation();Wait([&]{return A->StepCount()==33;});TestTrue(TEXT("gravity creates contact history from 1e-8m gap"),A->HistoryCount()>0);
 // ISM instance matrices use render precision; solver remains double precision.
 TestTrue(TEXT("instance follows solver"),A->ParticlePosition().Equals(A->SolverPositionCm(),1e-4));
 A->ToggleSimulation();Wait([&]{return A->IsRunning()&&A->StepCount()>8;});A->PauseSimulation();Wait([&]{return !A->IsRunning();});
 const auto Paused=A->StepCount();A->Tick(.5f);TestEqual(TEXT("paused no advancement"),A->StepCount(),Paused);
 const auto Old=A->RunGeneration();A->ResetSimulation();TestTrue(TEXT("generation increments"),A->RunGeneration()>Old);Wait([&]{return A->IsReady();});
 TestEqual(TEXT("reset clock"),A->StepCount(),uint64(0));TestEqual(TEXT("reset history"),A->HistoryCount(),0);
 A->ParticleCount=300;A->DiameterM=.04;A->DensityKgM3=2500;A->FixedDt=2e-7;A->NormalDamping=40;A->TangentDamping=10;A->WallNormalDamping=40;A->WallTangentDamping=10;A->InitialPositionCm=FVector(0,0,450);A->ResetSimulation();Wait([&]{return A->IsReady();});
 TestEqual(TEXT("300 instance display"),A->DisplayedCount(),300);A->ToggleSimulation();Wait([&]{return A->IsRunning()&&A->StepCount()>=8;});
 double MaxTick=0;int32 Updates=0;uint64 Previous=A->StepCount();for(int I=0;I<120;++I){const double Start=FPlatformTime::Seconds();A->Tick(1.f/120);MaxTick=FMath::Max(MaxTick,FPlatformTime::Seconds()-Start);if(A->StepCount()!=Previous){++Updates;Previous=A->StepCount();}FPlatformProcess::Sleep(.001f);}
 TestTrue(TEXT("fresh 300-particle snapshots consumed"),Updates>0);
 TestTrue(TEXT("render ticks remain responsive"),MaxTick<.1);AddInfo(FString::Printf(TEXT("300-particle max polling/render tick %.9fs, fresh snapshots %d"),MaxTick,Updates));
 for(int I=0;I<5;++I){A->ResetSimulation();Wait([&]{return A->IsReady();});TestEqual(TEXT("new run is step zero"),A->StepCount(),uint64(0));A->ToggleSimulation();}
 TestTrue(TEXT("Chaos remains disabled"),A->ChaosDisabled());
 // Destruction waits for the one in-flight solver step, with no UObject on the worker.
 TestTrue(TEXT("actor destruction accepted"),A->Destroy());TestFalse(TEXT("EndPlay released worker"),A->IsReady());W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDEMTrackingBridge,"Powder.DEM.TrackingBridge",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDEMTrackingBridge::RunTest(const FString& Parameters)
{
 UWorld::InitializationValues IV;IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);
 if(!TestNotNull(TEXT("tracking test world"),W))return false;
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 FString Text,Groups;const FString Dir=FPaths::ProjectDir()/TEXT("Physics/PowderCore/fixtures");
 if(!FFileHelper::LoadFileToString(Text,*(Dir/TEXT("L_Main.boxes")))||!FFileHelper::LoadFileToString(Groups,*(Dir/TEXT("L_Main.tracking")))){AddError(TEXT("missing tracking fixtures"));W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return false;}
 std::istringstream In(TCHAR_TO_UTF8(*Text)),Gin(TCHAR_TO_UTF8(*Groups));auto Boxes=powder::read_boxes(In);
 std::string Magic;int Version;Gin>>Magic>>Version;std::map<uint64,int> Roles;for(int K=0;K<4;++K){int N;Gin>>N;for(int I=0;I<N;++I){uint64 Id;Gin>>Id;Roles[Id]=K;}}
 const FName Tags[]={TEXT("PowderFeed"),TEXT("PowderFunnel"),TEXT("PowderTube"),TEXT("PowderCollector")};TArray<AStaticMeshActor*> Walls;
 for(const auto& B:Boxes){auto* Wall=W->SpawnActor<AStaticMeshActor>();Wall->Tags={TEXT("PowderRig"),Tags[Roles.at(B.id)]};
  Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
  const auto X=B.axes[0],Z=B.axes[2];const FQuat Q=FRotationMatrix::MakeFromXZ(FVector(X.x,X.y,X.z),FVector(Z.x,Z.y,Z.z)).ToQuat();
  Wall->SetActorTransform(FTransform(Q,FVector(B.centre_m.x,B.centre_m.y,B.centre_m.z)*100,FVector(B.half_m.x,B.half_m.y,B.half_m.z)*2));Walls.Add(Wall);
 }
 W->InitializeActorsForPlay(FURL());
 auto* A=W->SpawnActor<APowderDEMTest>();A->DispatchBeginPlay();TestTrue(TEXT("actor lifecycle initialized"),A->IsActorInitialized()&&A->HasActorBegunPlay());auto Wait=[&](){const double End=FPlatformTime::Seconds()+15;while(FPlatformTime::Seconds()<End){A->Tick(1.f/60);if(A->IsReady())return true;FPlatformProcess::Sleep(.001f);}AddError(TEXT("tracking snapshot timeout"));return false;};
 A->ResetSimulation();Wait();TestTrue(TEXT("actual 53-box rig tracking enabled"),A->TrackingEnabled());TestTrue(TEXT("rig settings valid"),A->Error.IsEmpty());TestEqual(TEXT("all 300 in feed"),A->TrackedCount(0),300);TestTrue(TEXT("mass balanced"),FMath::Abs(A->TrackedMassError())<1e-10);TestTrue(TEXT("ID accessible"),A->ParticleTracking(1).Contains(TEXT("Feed")));
 A->ParticleCount=1;A->InitialPositionCm=FVector(0,0,300);A->ResetSimulation();Wait();TestEqual(TEXT("tube centre classified"),A->TrackedCount(1),1);
 A->InitialPositionCm=FVector(0,0,150);A->ResetSimulation();Wait();TestEqual(TEXT("collector centre classified"),A->TrackedCount(2),1);
 A->InitialPositionCm=FVector(0,0,230);A->ResetSimulation();Wait();TestEqual(TEXT("flight gap classified outside"),A->TrackedCount(3),1);
 const FTransform Move(FRotator(0,90,0),FVector(100,200,300));for(auto* Wall:Walls){Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);TestTrue(TEXT("fixture relocation accepted"),Wall->SetActorTransform(Wall->GetActorTransform()*Move));Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);}
 A->InitialPositionCm=Move.TransformPosition(FVector(0,0,300));A->ResetSimulation();Wait();TestTrue(TEXT("moved rig valid"),A->Error.IsEmpty());TestEqual(TEXT("moved measurement volume follows geometry"),A->TrackedCount(1),1);TestTrue(TEXT("reset clears passage times"),A->ParticleTracking(1).Contains(TEXT("entry -1.")));
 TestTrue(TEXT("actor destruction accepted"),A->Destroy());TestFalse(TEXT("EndPlay released worker"),A->IsReady());W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDEMSettingsBridge,"Powder.DEM.SettingsBridge",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDEMSettingsBridge::RunTest(const FString& Parameters)
{
 UWorld::InitializationValues IV;IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);if(!TestNotNull(TEXT("settings world"),W))return false;
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* Floor=W->SpawnActor<AStaticMeshActor>();Floor->Tags.Add(TEXT("PowderRig"));Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Floor->SetActorLocation(FVector(0,0,-50));Floor->SetActorScale3D(FVector(4,4,1));
 W->InitializeActorsForPlay(FURL());auto* A=W->SpawnActor<APowderDEMTest>();A->DispatchBeginPlay();
 auto Wait=[&](){const double End=FPlatformTime::Seconds()+15;while(FPlatformTime::Seconds()<End){A->Tick(1.f/60);if(A->IsReady())return true;FPlatformProcess::Sleep(.001f);}AddError(TEXT("settings worker timeout"));return false;};
 A->SettingsFile=TEXT("Automation_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".dem");
 A->SizeDistribution=EDEMSizeDistribution::UniformNumber;A->RandomSeed=123;A->ParticleCount=60;A->ContactLaw=EDEMContactLaw::HertzMindlin;A->WallYoungPa=4e9;A->ResetSimulation();Wait();TestTrue(TEXT("distributed Hertz initialization valid"),A->Error.IsEmpty());TestEqual(TEXT("generated count"),A->DisplayedCount(),60);
 const FVector Position=A->SolverPositionCm();TestTrue(TEXT("save settings"),A->SaveSettings());
 A->ParticleCount=3;A->RandomSeed=999;A->WallYoungPa=1e5;TestTrue(TEXT("load settings"),A->LoadSettings());Wait();TestTrue(TEXT("loaded configuration valid"),A->Error.IsEmpty());TestEqual(TEXT("seed restored"),A->RandomSeed,int64(123));TestEqual(TEXT("wall material restored"),A->WallYoungPa,4e9);TestEqual(TEXT("count restored"),A->DisplayedCount(),60);TestTrue(TEXT("loaded placement reproducible"),Position.Equals(A->SolverPositionCm(),1e-10));
 const FString Path=FPaths::ProjectSavedDir()/TEXT("DEM")/A->SettingsFile;FString Saved;FFileHelper::LoadFileToString(Saved,*Path);
 A->DensityKgM3=-1;TestFalse(TEXT("invalid settings cannot overwrite file"),A->SaveSettings());FString Unchanged;FFileHelper::LoadFileToString(Unchanged,*Path);TestEqual(TEXT("old settings file retained"),Unchanged,Saved);A->DensityKgM3=2500;
 FFileHelper::SaveStringToFile(TEXT("POWDER_SETTINGS 999"),*Path);const auto Generation=A->RunGeneration();TestFalse(TEXT("bad schema rejected"),A->LoadSettings());TestEqual(TEXT("bad load does not reset current run"),A->RunGeneration(),Generation);TestEqual(TEXT("bad load keeps current fields"),A->RandomSeed,int64(123));
 A->SettingsFile=TEXT("../invalid.dem");TestFalse(TEXT("path traversal rejected"),A->SaveSettings());
 A->AmountMode=EDEMAmountMode::Mass;A->TargetMassKg=1;A->MassToleranceKg=.1;A->ResetSimulation();Wait();TestTrue(TEXT("mass mode initialized"),A->Error.IsEmpty());TestTrue(TEXT("mass mode generates complete particles"),A->DisplayedCount()>0&&A->DisplayedCount()<60);
 // Expected diagnostic: user gets a reason and no partially generated display.
 AddExpectedError(TEXT("DEM worker: initial placement failed"),EAutomationExpectedErrorFlags::Contains,1);A->MaxPlacementCandidates=1;A->ResetSimulation();Wait();TestFalse(TEXT("impossible placement reported"),A->Error.IsEmpty());TestEqual(TEXT("no partial display on failure"),A->DisplayedCount(),0);
 A->MaxPlacementCandidates=100000;A->ResetSimulation();Wait();TestTrue(TEXT("reset recovers after placement error"),A->Error.IsEmpty());
 IFileManager::Get().Delete(*Path);A->Destroy();TestFalse(TEXT("settings worker released"),A->IsReady());W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDEMSupplyBridge,"Powder.DEM.SupplyBridge",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDEMSupplyBridge::RunTest(const FString& Parameters)
{
 UWorld::InitializationValues IV;IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);if(!TestNotNull(TEXT("supply world"),W))return false;
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* Floor=W->SpawnActor<AStaticMeshActor>();Floor->Tags.Add(TEXT("PowderRig"));Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Floor->SetActorLocation(FVector(0,0,-50));Floor->SetActorScale3D(FVector(4,4,1));
 W->InitializeActorsForPlay(FURL());auto* A=W->SpawnActor<APowderDEMTest>();A->DispatchBeginPlay();
 auto Wait=[&](TFunctionRef<bool()> Done){const double End=FPlatformTime::Seconds()+15;while(FPlatformTime::Seconds()<End){A->Tick(1.f/60);if(Done())return true;FPlatformProcess::Sleep(.001f);}AddError(TEXT("supply worker timeout"));return false;};
 A->SupplyMode=EDEMSupplyMode::Timed;A->SupplyStartSeconds=2*A->FixedDt;A->SupplyIntervalSeconds=2*A->FixedDt;A->SupplyRetrySeconds=A->FixedDt;A->ParticlesPerEvent=1;A->ParticleCount=3;A->SpawnHalfExtentsCm=FVector(2.1);A->GravityMps2=FVector::ZeroVector;A->ResetSimulation();Wait([&]{return A->IsReady();});
 TestTrue(TEXT("timed valid"),A->Error.IsEmpty());TestEqual(TEXT("no future particles displayed"),A->DisplayedCount(),0);
 A->StepSimulation();Wait([&]{return A->StepCount()==1;});TestEqual(TEXT("no early supply"),A->RequestedCount(),0);
 A->StepSimulation();Wait([&]{return A->StepCount()==2;});TestEqual(TEXT("first scheduled particle rendered"),A->DisplayedCount(),1);
 for(int I=0;I<4;++I)A->StepSimulation();Wait([&]{return A->StepCount()==6;});TestEqual(TEXT("three requested"),A->RequestedCount(),3);TestEqual(TEXT("two waiting"),A->PendingCount(),2);TestEqual(TEXT("no forced overlaps"),A->DisplayedCount(),1);TestTrue(TEXT("supply mass balanced"),FMath::Abs(A->SupplyMassError())<1e-10);TestTrue(TEXT("birth tracking accessible"),!A->ParticleTracking(1).Contains(TEXT("no current record")));
 A->SettingsFile=TEXT("SupplyAutomation_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".dem");TestTrue(TEXT("supply saved"),A->SaveSettings());A->SupplyMode=EDEMSupplyMode::Batch;A->ParticlesPerEvent=5;TestTrue(TEXT("supply loaded"),A->LoadSettings());Wait([&]{return A->IsReady();});TestTrue(TEXT("supply mode restored"),A->SupplyMode==EDEMSupplyMode::Timed);TestEqual(TEXT("event size restored"),A->ParticlesPerEvent,1);TestEqual(TEXT("reset clears backlog"),A->PendingCount(),0);TestEqual(TEXT("reset clears born particles"),A->DisplayedCount(),0);
 A->SupplyMode=EDEMSupplyMode::Batch;A->SpawnHalfExtentsCm=FVector(18.5,18.5,35);A->ResetSimulation();Wait([&]{return A->IsReady();});TestEqual(TEXT("delayed batch empty"),A->DisplayedCount(),0);A->StepSimulation();A->StepSimulation();Wait([&]{return A->StepCount()==2;});TestEqual(TEXT("entire delayed batch displayed"),A->DisplayedCount(),3);TestEqual(TEXT("batch no backlog"),A->PendingCount(),0);TestTrue(TEXT("batch valid"),A->Error.IsEmpty());
 IFileManager::Get().Delete(*(FPaths::ProjectSavedDir()/TEXT("DEM")/A->SettingsFile));A->Destroy();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDEMResultsBridge,"Powder.DEM.ResultsBridge",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDEMResultsBridge::RunTest(const FString& Parameters)
{
 UWorld::InitializationValues IV;IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);if(!TestNotNull(TEXT("results world"),W))return false;
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* Floor=W->SpawnActor<AStaticMeshActor>();Floor->Tags.Add(TEXT("PowderRig"));Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Floor->SetActorLocation(FVector(0,0,-50));Floor->SetActorScale3D(FVector(4,4,1));
 W->InitializeActorsForPlay(FURL());auto* A=W->SpawnActor<APowderDEMTest>();A->DispatchBeginPlay();
 auto Wait=[&](TFunctionRef<bool()> Done){const double End=FPlatformTime::Seconds()+15;while(FPlatformTime::Seconds()<End){A->Tick(1.f/60);if(Done())return true;FPlatformProcess::Sleep(.001f);}AddError(TEXT("results worker timeout"));return false;};
 A->ParticleCount=1;A->ResultSampleSeconds=A->FixedDt;A->MaxResultSamples=2;A->ResetSimulation();Wait([&]{return A->IsReady();});TestTrue(TEXT("valid results configuration"),A->Error.IsEmpty());A->ToggleSimulation();Wait([&]{return A->ResultStatus().Contains(TEXT("LIMIT"));});TestEqual(TEXT("record limit stops at step 2"),A->StepCount(),uint64(2));TestFalse(TEXT("limit paused"),A->IsRunning());TestTrue(TEXT("export at limit"),A->ExportResults());
 const FString First=A->LastResultsDirectory;TestTrue(TEXT("completion marker exists"),IFileManager::Get().FileExists(*(First/TEXT("complete.json"))));
 for(const TCHAR* Name:{TEXT("metadata.json"),TEXT("summary.json"),TEXT("timeseries.csv"),TEXT("particles.csv"),TEXT("events.csv"),TEXT("settings.dem"),TEXT("run.json")})TestTrue(TEXT("bundle file exists"),IFileManager::Get().FileExists(*(First/Name)));
 FString Metadata;FFileHelper::LoadFileToString(Metadata,*(First/TEXT("metadata.json")));TestTrue(TEXT("actual code version recorded"),Metadata.Contains(TEXT("0.14.0")));TestTrue(TEXT("record limit recorded"),Metadata.Contains(TEXT("\"sample_limit_reached\":true")));
 A->ResultSampleSeconds=999;TestTrue(TEXT("repeat export unique destination"),A->ExportResults());const FString Second=A->LastResultsDirectory;TestTrue(TEXT("previous export never overwritten"),Second!=First);FString Again;FFileHelper::LoadFileToString(Again,*(Second/TEXT("metadata.json")));TestEqual(TEXT("active snapshot criteria, not uncommitted UI fields"),Again,Metadata);
 A->MaxResultSamples=10000;A->ResetSimulation();Wait([&]{return A->IsReady();});A->ToggleSimulation();Wait([&]{return A->IsRunning();});TestFalse(TEXT("running export rejected"),A->ExportResults());TestEqual(TEXT("failed export keeps previous destination"),A->LastResultsDirectory,Second);A->PauseSimulation();Wait([&]{return !A->IsRunning();});
 TestTrue(TEXT("paused off-grid export"),A->ExportResults());const FString Third=A->LastResultsDirectory;TestTrue(TEXT("new run resets monitoring"),!A->ResultStatus().Contains(TEXT("LIMIT")));
 for(const auto& Dir:{First,Second,Third}){TestTrue(TEXT("test output under DEM results"),Dir.StartsWith(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("DEM/Results"))));IFileManager::Get().DeleteDirectory(*Dir,false,true);}
 A->Destroy();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
#endif
