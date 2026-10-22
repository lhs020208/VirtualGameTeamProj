#include "PowderDEMTest.h"
#include "PowderClumpTest.h"
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
#include "powder/clump.hpp"
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
 FString Metadata;FFileHelper::LoadFileToString(Metadata,*(First/TEXT("metadata.json")));TestTrue(TEXT("actual code version recorded"),Metadata.Contains(TEXT("0.21.0")));TestTrue(TEXT("record limit recorded"),Metadata.Contains(TEXT("\"sample_limit_reached\":true")));
 A->ResultSampleSeconds=999;TestTrue(TEXT("repeat export unique destination"),A->ExportResults());const FString Second=A->LastResultsDirectory;TestTrue(TEXT("previous export never overwritten"),Second!=First);FString Again;FFileHelper::LoadFileToString(Again,*(Second/TEXT("metadata.json")));TestEqual(TEXT("active snapshot criteria, not uncommitted UI fields"),Again,Metadata);
 A->MaxResultSamples=10000;A->ResetSimulation();Wait([&]{return A->IsReady();});A->ToggleSimulation();Wait([&]{return A->IsRunning();});TestFalse(TEXT("running export rejected"),A->ExportResults());TestEqual(TEXT("failed export keeps previous destination"),A->LastResultsDirectory,Second);A->PauseSimulation();Wait([&]{return !A->IsRunning();});
 TestTrue(TEXT("paused off-grid export"),A->ExportResults());const FString Third=A->LastResultsDirectory;TestTrue(TEXT("new run resets monitoring"),!A->ResultStatus().Contains(TEXT("LIMIT")));
 for(const auto& Dir:{First,Second,Third}){TestTrue(TEXT("test output under DEM results"),Dir.StartsWith(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("DEM/Results"))));IFileManager::Get().DeleteDirectory(*Dir,false,true);}
 A->Destroy();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDEMRollingSettings,"Powder.DEM.RollingSettings",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDEMRollingSettings::RunTest(const FString& Parameters)
{
 UWorld::InitializationValues IV;IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);if(!TestNotNull(TEXT("rolling world"),W))return false;
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* Floor=W->SpawnActor<AStaticMeshActor>();Floor->Tags.Add(TEXT("PowderRig"));Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Floor->SetActorLocation(FVector(0,0,-50));Floor->SetActorScale3D(FVector(4,4,1));
 W->InitializeActorsForPlay(FURL());
 auto* A=W->SpawnActor<APowderDEMTest>();A->DispatchBeginPlay();A->ParticleCount=1;
 A->PairRollingStiffness=2000;A->PairRollingDamping=3;A->PairRollingFriction=.1;A->WallRollingStiffness=3000;A->WallRollingDamping=5;A->WallRollingFriction=.2;
 A->SettingsFile=TEXT("Task16_")+FGuid::NewGuid().ToString()+TEXT(".dem");A->ResetSimulation();
 auto Wait=[&]{const double End=FPlatformTime::Seconds()+15;while(FPlatformTime::Seconds()<End){A->Tick(1.f/60);if(A->IsReady())return true;FPlatformProcess::Sleep(.001f);}AddError(TEXT("rolling worker timeout"));return false;};
 Wait();TestTrue(TEXT("rolling configuration valid"),A->Error.IsEmpty());TestTrue(TEXT("save rolling settings"),A->SaveSettings());
 A->PairRollingFriction=0;A->WallRollingFriction=0;TestTrue(TEXT("load rolling settings"),A->LoadSettings());Wait();
 TestEqual(TEXT("pair coefficient restored"),A->PairRollingFriction,.1);TestEqual(TEXT("wall coefficient restored"),A->WallRollingFriction,.2);TestEqual(TEXT("wall stiffness restored"),A->WallRollingStiffness,3000.);
 TestTrue(TEXT("rolling settings exported"),A->ExportResults());FString Text;FFileHelper::LoadFileToString(Text,*(A->LastResultsDirectory/TEXT("settings.dem")));TestTrue(TEXT("new settings schema exported"),Text.Contains(TEXT("POWDER_SETTINGS 4")));TestTrue(TEXT("rolling model exported"),Text.Contains(TEXT("wall_rolling_sds 3000 5")));
 if(!A->LastResultsDirectory.IsEmpty())IFileManager::Get().DeleteDirectory(*A->LastResultsDirectory,false,true);
 IFileManager::Get().Delete(*(FPaths::ProjectSavedDir()/TEXT("DEM")/A->SettingsFile));
 A->Destroy();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDEMAdhesionSettings,"Powder.DEM.AdhesionSettings",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDEMAdhesionSettings::RunTest(const FString& Parameters)
{
 UWorld::InitializationValues IV;IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);if(!TestNotNull(TEXT("rolling world"),W))return false;
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* Floor=W->SpawnActor<AStaticMeshActor>();Floor->Tags.Add(TEXT("PowderRig"));Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Floor->SetActorLocation(FVector(0,0,-50));Floor->SetActorScale3D(FVector(4,4,1));
 W->InitializeActorsForPlay(FURL());
 auto* A=W->SpawnActor<APowderDEMTest>();A->DispatchBeginPlay();A->ParticleCount=1;
 A->PairAdhesionGamma=.1;A->WallAdhesionGamma=.2;A->PairRollingStiffness=2000;A->PairRollingDamping=3;A->PairRollingFriction=.1;A->WallRollingStiffness=3000;A->WallRollingDamping=5;A->WallRollingFriction=.2;
 A->SettingsFile=TEXT("Task17_")+FGuid::NewGuid().ToString()+TEXT(".dem");A->ResetSimulation();
 auto Wait=[&]{const double End=FPlatformTime::Seconds()+15;while(FPlatformTime::Seconds()<End){A->Tick(1.f/60);if(A->IsReady())return true;FPlatformProcess::Sleep(.001f);}AddError(TEXT("rolling worker timeout"));return false;};
 Wait();TestTrue(TEXT("rolling configuration valid"),A->Error.IsEmpty());TestTrue(TEXT("save rolling settings"),A->SaveSettings());
 A->PairAdhesionGamma=0;A->WallAdhesionGamma=0;A->PairRollingFriction=0;A->WallRollingFriction=0;TestTrue(TEXT("load rolling settings"),A->LoadSettings());Wait();
 TestEqual(TEXT("pair adhesion restored"),A->PairAdhesionGamma,.1);TestEqual(TEXT("wall adhesion restored"),A->WallAdhesionGamma,.2);TestEqual(TEXT("pair coefficient restored"),A->PairRollingFriction,.1);TestEqual(TEXT("wall coefficient restored"),A->WallRollingFriction,.2);TestEqual(TEXT("wall stiffness restored"),A->WallRollingStiffness,3000.);
 TestTrue(TEXT("rolling settings exported"),A->ExportResults());FString Text;FFileHelper::LoadFileToString(Text,*(A->LastResultsDirectory/TEXT("settings.dem")));TestTrue(TEXT("new settings schema exported"),Text.Contains(TEXT("POWDER_SETTINGS 4")));TestTrue(TEXT("adhesion exported"),Text.Contains(TEXT("contact_adhesion_gamma_j_m2 0.1")));TestTrue(TEXT("rolling model exported"),Text.Contains(TEXT("wall_rolling_sds 3000 5")));
 if(!A->LastResultsDirectory.IsEmpty())IFileManager::Get().DeleteDirectory(*A->LastResultsDirectory,false,true);
 IFileManager::Get().Delete(*(FPaths::ProjectSavedDir()/TEXT("DEM")/A->SettingsFile));
 A->Destroy();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}



IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDEMClumpBridge,"Powder.DEM.ClumpBridge",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDEMClumpBridge::RunTest(const FString& Parameters){
 UWorld::InitializationValues IV;IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);if(!TestNotNull(TEXT("clump world"),W))return false;GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());auto* A=W->SpawnActor<APowderClumpTest>();A->DispatchBeginPlay();TestTrue(TEXT("clump initialization"),A->Error.IsEmpty());TestEqual(TEXT("body count distinct from render count"),A->BodyCount(),6);TestEqual(TEXT("render all rigid members"),A->RenderedPebbles(),9);auto Position=A->BodyPositionCm(1);A->StepSimulation();TestTrue(TEXT("fixed physical step"),A->PhysicalSeconds==A->FixedDt);TestTrue(TEXT("COM gravity movement"),A->BodyPositionCm(1).Z<Position.Z);TestTrue(TEXT("paused body export"),A->ExportState());FString Text;FFileHelper::LoadFileToString(Text,*(A->ExportDirectory/TEXT("pebbles.csv")));TestTrue(TEXT("shape geometry exported"),Text.Contains(TEXT("local_x_m")));auto Dir=A->ExportDirectory;A->InclusionDensity=1000;A->ResetSimulation();TestTrue(TEXT("material reset valid"),A->Error.IsEmpty());TestEqual(TEXT("reset physical clock"),A->PhysicalSeconds,0.);A->ToggleSimulation();TestFalse(TEXT("running export rejected"),A->ExportState());A->ToggleSimulation();A->FixedDt=1;A->ResetSimulation();TestTrue(TEXT("invalid clump dt rejected"),!A->Error.IsEmpty());TestEqual(TEXT("invalid reset no stale instances"),A->RenderedPebbles(),0);if(!Dir.IsEmpty())IFileManager::Get().DeleteDirectory(*Dir,false,true);A->Destroy();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDEMMeshBridge,"Powder.DEM.MeshBridge",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDEMMeshBridge::RunTest(const FString& Parameters){
 UWorld::InitializationValues IV;IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);if(!TestNotNull(TEXT("mesh world"),W))return false;GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());
 auto* A=W->SpawnActor<APowderClumpTest>();A->DispatchBeginPlay();A->bTriangleMeshRig=true;A->ResetSimulation();TestTrue(TEXT("actual render triangles converted to SI"),A->Error.IsEmpty());TestEqual(TEXT("six rendered Cube LOD0 meshes each 48 triangles"),A->MeshTriangleCount(),288);TestEqual(TEXT("same six rigid bodies"),A->BodyCount(),6);TestEqual(TEXT("same nine visible pebbles"),A->RenderedPebbles(),9);TestTrue(TEXT("geometry diagnostics"),A->GeometryDiagnostics.Contains(TEXT("coplanar_patches 6")));
 auto Before=A->BodyPositionCm(0);A->StepSimulation();TestTrue(TEXT("mesh fixture falling in solver"),A->BodyPositionCm(0).Z<Before.Z);TestTrue(TEXT("mesh physical time"),A->PhysicalSeconds==A->FixedDt);TestTrue(TEXT("export analysis geometry"),A->ExportState());FString Geometry;FFileHelper::LoadFileToString(Geometry,*(A->ExportDirectory/TEXT("surface_0.mesh")));TestTrue(TEXT("export metres"),Geometry.StartsWith(TEXT("POWDER_TRIANGLES 1 1 ")));auto Dir=A->ExportDirectory;
 // Run the actual exported render triangles through the solver, not just a synthetic box.
 try{auto World=powder::clump_demo();World.boxes.clear();for(int32 I=0;I<6;++I){FString Text;TestTrue(TEXT("surface file exists"),FFileHelper::LoadFileToString(Text,*(Dir/FString::Printf(TEXT("surface_%d.mesh"),I))));std::istringstream Stream(TCHAR_TO_UTF8(*Text));World.meshes.emplace_back(powder::read_triangle_mesh(Stream));}
 for(auto& B:World.bodies)B.position_m.z+=.30;std::size_t Hits=0;double Mass=0;for(int32 I=0;I<150000;++I){auto Report=World.step(2e-6);Hits+=Report.wall_contacts;Mass=Report.mass_kg;}TestTrue(TEXT("actual mesh ramp/tube contact after 0.3 seconds"),Hits>0);TestTrue(TEXT("mass conserved through mesh rig"),FMath::Abs(Mass-3*.606536821173063)<1e-6);for(const auto& B:World.bodies)TestTrue(TEXT("no floor leak in mesh rig"),B.position_m.z>0);
 AddInfo(FString::Printf(TEXT("Mesh fixture 0.3 s: contacts=%llu, mass=%.17g kg"),static_cast<uint64>(Hits),Mass));
 }catch(const std::exception& E){AddError(FString::Printf(TEXT("Actual mesh drop failed: %s"),UTF8_TO_TCHAR(E.what())));}
 A->bTriangleMeshRig=false;A->ResetSimulation();TestEqual(TEXT("reset back to analytic mode"),A->MeshTriangleCount(),0);TestTrue(TEXT("analytic restored"),A->Error.IsEmpty());A->bTriangleMeshRig=true;A->SetActorScale3D(FVector(100));A->ResetSimulation();TestFalse(TEXT("unsupported fixture scale rejected"),A->Error.IsEmpty());TestEqual(TEXT("no stale particles after bad geometry"),A->RenderedPebbles(),0);
 if(!Dir.IsEmpty())IFileManager::Get().DeleteDirectory(*Dir,false,true);A->Destroy();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDEMPaddleBridge,"Powder.DEM.PaddleBridge",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDEMPaddleBridge::RunTest(const FString& Parameters){
 UWorld::InitializationValues IV;IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);if(!TestNotNull(TEXT("paddle world"),W))return false;GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());auto* A=W->SpawnActor<APowderClumpTest>();A->DispatchBeginPlay();A->bPaddleRig=true;A->ResetSimulation();TestTrue(TEXT("paddle initialization"),A->Error.IsEmpty());TestEqual(TEXT("static tray + moving blade render triangles"),A->MeshTriangleCount(),288);TestEqual(TEXT("applied target RPM"),A->AppliedPaddleRPM(),30.);
 A->StepSimulation();auto Expected=FQuat(FVector::UpVector,30.*2*PI/60*A->PhysicalSeconds);TestTrue(TEXT("display pose uses same physical time"),A->DisplayedPaddleRotation().Equals(Expected,1e-12));auto Time=A->PhysicalSeconds;auto Q=A->DisplayedPaddleRotation();A->Tick(1.f);TestEqual(TEXT("paused rendering does not advance physics"),A->PhysicalSeconds,Time);TestTrue(TEXT("paused blade does not spin"),A->DisplayedPaddleRotation().Equals(Q,1e-12));A->PaddleRPM=60;A->StepSimulation();TestEqual(TEXT("UI change not applied until reset"),A->AppliedPaddleRPM(),30.);TestTrue(TEXT("export moving geometry and work"),A->ExportState());auto Dir=A->ExportDirectory;FString State;FFileHelper::LoadFileToString(State,*(Dir/TEXT("paddle_state.csv")));TestTrue(TEXT("power bookkeeping exported"),State.Contains(TEXT("wall_work_to_particles_J")));
 try{auto World=powder::clump_demo();World.boxes.clear();for(auto& B:World.bodies){B.position_m.z=.022;B.orientation={};}for(int32 I=0;I<5;++I){FString Text;FFileHelper::LoadFileToString(Text,*(Dir/FString::Printf(TEXT("surface_%d.mesh"),I)));std::istringstream Stream(TCHAR_TO_UTF8(*Text));World.meshes.emplace_back(powder::read_triangle_mesh(Stream));}FString Text;FFileHelper::LoadFileToString(Text,*(Dir/TEXT("moving_reference_0.mesh")));std::istringstream Stream(TCHAR_TO_UTF8(*Text));powder::PrescribedMotion Motion;Motion.pivot_m={0,0,.04};Motion.rpm=30;World.moving_meshes.emplace_back(powder::StaticTriangleMesh(powder::read_triangle_mesh(Stream)),Motion);double MaxTorque=0;std::size_t Contacts=0;for(int32 I=0;I<150000;++I){auto R=World.step(2e-6);Contacts+=R.wall_contacts;MaxTorque=FMath::Max(MaxTorque,FMath::Abs(R.moving_wall_loads[0].reaction_torque_nm.z));}TestTrue(TEXT("blade transfers actual work"),World.moving_wall_work_j>0);TestTrue(TEXT("nonzero paddle torque"),MaxTorque>0);for(const auto& B:World.bodies)TestTrue(TEXT("no floor leakage"),B.position_m.z>0);AddInfo(FString::Printf(TEXT("Paddle 30 RPM / 0.3 s: work=%.17g J, peak |Tz|=%.17g Nm, wall contact observations=%llu"),World.moving_wall_work_j,MaxTorque,static_cast<uint64>(Contacts)));}catch(const std::exception& E){AddError(FString::Printf(TEXT("Paddle fixture failed: %s"),UTF8_TO_TCHAR(E.what())));}
 A->PaddleRPM=-30;A->ResetSimulation();TestEqual(TEXT("reset reverse RPM"),A->AppliedPaddleRPM(),-30.);TestEqual(TEXT("reset work"),A->WallWorkJ,0.);A->StepSimulation();TestTrue(TEXT("reverse display rotation"),A->DisplayedPaddleRotation().Equals(FQuat(FVector::UpVector,-30.*2*PI/60*A->PhysicalSeconds),1e-12));A->PaddleRPM=1e9;A->ResetSimulation();TestFalse(TEXT("unsafe paddle speed rejected"),A->Error.IsEmpty());TestEqual(TEXT("invalid reset clears bodies"),A->BodyCount(),0);if(!Dir.IsEmpty())IFileManager::Get().DeleteDirectory(*Dir,false,true);A->Destroy();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDEMMixingBridge,"Powder.DEM.MixingBridge",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDEMMixingBridge::RunTest(const FString& Parameters){
 UWorld::InitializationValues IV;IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);if(!TestNotNull(TEXT("mixing world"),W))return false;GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());auto* A=W->SpawnActor<APowderClumpTest>();A->DispatchBeginPlay();A->bMixingRig=true;A->MixingDurationSeconds=4*A->FixedDt;A->MixingSampleSeconds=2*A->FixedDt;A->ResetSimulation();
 TestTrue(TEXT("mixing initialization"),A->Error.IsEmpty());TestEqual(TEXT("charge count"),A->BodyCount(),32);TestEqual(TEXT("all charge visible"),A->RenderedPebbles(),32);TestEqual(TEXT("separate B display group"),A->RenderedGroupB(),16);TestTrue(TEXT("separated reference valid"),A->bMixingIndexValid);TestTrue(TEXT("separated index zero"),FMath::Abs(A->MixingIndex)<1e-12);TestTrue(TEXT("charge mass SI"),FMath::Abs(A->ChargeMassKg-32*2500*3.14159265358979323846/6*FMath::Pow(.04,3))<1e-12);TestEqual(TEXT("initial sample"),A->MixingSampleCount(),1);
 A->MixingDurationSeconds=1;A->GroupACount=128;A->PaddleRPM=60;for(int32 I=0;I<8;++I)A->StepSimulation();TestTrue(TEXT("applied duration stops exactly"),A->bMixingFinished);TestTrue(TEXT("physical end time"),FMath::Abs(A->PhysicalSeconds-8e-6)<1e-15);TestEqual(TEXT("initial middle final samples"),A->MixingSampleCount(),3);TestEqual(TEXT("charge config snapshotted"),A->BodyCount(),32);TestEqual(TEXT("RPM snapshotted"),A->AppliedPaddleRPM(),30.);A->ToggleSimulation();TestFalse(TEXT("completed experiment cannot resume past limit"),A->bRunning);
 TestTrue(TEXT("mixing export"),A->ExportState());auto Dir=A->ExportDirectory;FString Text;FFileHelper::LoadFileToString(Text,*(Dir/TEXT("mixing_metadata.json")));TestTrue(TEXT("defined metric exported"),Text.Contains(TEXT("mass_weighted_cell_uniformity_v1")));TestTrue(TEXT("finished export metadata"),Text.Contains(TEXT("\"run_finished\":true")));TestTrue(TEXT("applied count exported"),Text.Contains(TEXT("\"count_a\":16")));FFileHelper::LoadFileToString(Text,*(Dir/TEXT("mixing_cells.csv")));TestTrue(TEXT("spatial mass and counts exported"),Text.Contains(TEXT("count_a,count_b,mass_a_kg,mass_b_kg")));
 A->GroupACount=16;A->PaddleRPM=30;A->bStartPremixed=true;A->MixingDurationSeconds=.5;A->MixingSampleSeconds=.05;A->ResetSimulation();TestTrue(TEXT("premixed reference"),A->Error.IsEmpty()&&A->bMixingIndexValid&&FMath::Abs(A->MixingIndex-1)<1e-12);TestFalse(TEXT("reset finished flag"),A->bMixingFinished);
 A->MixingGrid=FIntVector(8,8,1);A->ResetSimulation();TestTrue(TEXT("sparse grid still simulates"),A->Error.IsEmpty());TestFalse(TEXT("sparse score invalid"),A->bMixingIndexValid);A->MixingGrid.X=0;A->ResetSimulation();TestFalse(TEXT("invalid grid rejected"),A->Error.IsEmpty());TestEqual(TEXT("failed reset clears both render groups"),A->RenderedPebbles(),0);
 if(!Dir.IsEmpty())IFileManager::Get().DeleteDirectory(*Dir,false,true);A->Destroy();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDEMRealtimePacing,"Powder.DEM.RealtimePacing",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDEMRealtimePacing::RunTest(const FString& Parameters){
 UWorld::InitializationValues IV;IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);if(!TestNotNull(TEXT("pacing world"),W))return false;
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());
 auto* A=W->SpawnActor<APowderClumpTest>();A->DispatchBeginPlay();A->bMixingRig=true;A->ResetSimulation();
 TestTrue(TEXT("mixing initialized"),A->Error.IsEmpty());TestEqual(TEXT("one physical second per real second target"),A->TargetSimulationRate,1.);
 const double Dt=A->FixedDt;A->ToggleSimulation();const double Start=FPlatformTime::Seconds();
 do{A->Tick(1.f/60);}while(FPlatformTime::Seconds()-Start<1.1&&A->bRunning);
 const double Wall=FPlatformTime::Seconds()-Start,Physical=A->PhysicalSeconds;
 TestTrue(TEXT("real calculation advanced"),Physical>0&&A->Error.IsEmpty());TestEqual(TEXT("unchanged integration step"),A->FixedDt,Dt);
 TestTrue(TEXT("does not fake elapsed physical time"),Physical<=Wall+Dt);
 TestTrue(TEXT("actual throughput reported"),A->ActualSimulationRate>0&&FMath::IsFinite(A->ActualSimulationRate));
 AddInfo(FString::Printf(TEXT("PACING_BENCHMARK: 32 bodies, physical %.9g s, wall %.9g s, rate %.9g x, fixed dt %.9g s, lag %.9g s"),Physical,Wall,Physical/Wall,Dt,A->SimulationLagSeconds));
 A->ToggleSimulation();const double Paused=A->PhysicalSeconds;A->Tick(1);TestEqual(TEXT("pause barrier does not advance"),A->PhysicalSeconds,Paused);
 FPlatformProcess::Sleep(.02f);A->Tick(100);TestEqual(TEXT("engine delta never drives physics"),A->PhysicalSeconds,Paused);
 A->StepSimulation();TestTrue(TEXT("manual step still exactly FixedDt"),FMath::Abs(A->PhysicalSeconds-Paused-Dt)<1e-12);
 A->ResetSimulation();TestEqual(TEXT("reset clears lag"),A->SimulationLagSeconds,0.);TestEqual(TEXT("reset clears speed"),A->ActualSimulationRate,0.);
 A->bRealtimePacing=false;A->StepsPerFrame=3;A->ComputeBudgetMs=100;A->ToggleSimulation();FPlatformProcess::Sleep(.03f);A->ToggleSimulation();TestTrue(TEXT("worker advances without any render ticks"),A->PhysicalSeconds>3*Dt);
 A->Destroy();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDEMProfileBridge,"Powder.DEM.OptimizationProfile",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDEMProfileBridge::RunTest(const FString& Parameters){
 UWorld::InitializationValues IV;IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);if(!TestNotNull(TEXT("profile world"),W))return false;
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());
 auto* A=W->SpawnActor<APowderClumpTest>();auto* B=W->SpawnActor<APowderClumpTest>();A->DispatchBeginPlay();B->DispatchBeginPlay();
 A->bMixingRig=B->bMixingRig=true;A->bProfilePerformance=true;A->ResetSimulation();B->ResetSimulation();
 const double Start=FPlatformTime::Seconds();for(int I=0;I<500;++I){A->StepSimulation();B->StepSimulation();}
 TestTrue(TEXT("profile enabled and disabled no physics errors"),A->Error.IsEmpty()&&B->Error.IsEmpty());TestEqual(TEXT("same physical time"),A->PhysicalSeconds,B->PhysicalSeconds);
 for(int I=0;I<A->BodyCount();++I)TestTrue(TEXT("same body position with clocks"),A->BodyPositionCm(I).Equals(B->BodyPositionCm(I),0));
 TestTrue(TEXT("physics and display timings collected"),A->ProfiledPhysicsSeconds>0&&A->ProfiledDisplaySeconds>0&&A->ProfiledDisplayUpdates==501);TestEqual(TEXT("disabled profiling has no counters"),B->ProfiledDisplayUpdates,0);
 AddInfo(FString::Printf(TEXT("OPT01_UNREAL_CPU: 500 fixed steps, physics %.9g s, display_sync %.9g s, paired_loop %.9g s; display_sync excludes GPU drawing"),A->ProfiledPhysicsSeconds,A->ProfiledDisplaySeconds,FPlatformTime::Seconds()-Start));
 TestTrue(TEXT("profile export"),A->ExportState());const auto Dir=A->ExportDirectory;FString Text;TestTrue(TEXT("CSV profiling output"),FFileHelper::LoadFileToString(Text,*(Dir/TEXT("performance.csv")))&&Text.Contains(TEXT("display_sync_cpu"))&&Text.Contains(TEXT("post_step_guards")));
 A->ResetSimulation();TestEqual(TEXT("reset clears physics profile"),A->ProfiledPhysicsSeconds,0.);TestEqual(TEXT("reset begins one new display update"),A->ProfiledDisplayUpdates,1);
 if(!Dir.IsEmpty())IFileManager::Get().DeleteDirectory(*Dir,false,true);A->Destroy();B->Destroy();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDEMWorkerDisplay,"Powder.DEM.WorkerDisplay",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDEMWorkerDisplay::RunTest(const FString& Parameters){
 UWorld::InitializationValues IV;IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);if(!TestNotNull(TEXT("worker display world"),W))return false;
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());
 auto* A=W->SpawnActor<APowderClumpTest>();auto* B=W->SpawnActor<APowderClumpTest>();A->DispatchBeginPlay();B->DispatchBeginPlay();
 A->bMixingRig=B->bMixingRig=true;A->bRealtimePacing=B->bRealtimePacing=false;A->bProfilePerformance=B->bProfilePerformance=true;
 A->MixingDurationSeconds=B->MixingDurationSeconds=2000*A->FixedDt;A->MixingSampleSeconds=B->MixingSampleSeconds=200*A->FixedDt;
 A->MaxDisplayedPebbles=8;B->MaxDisplayedPebbles=0;A->ResetSimulation();B->ResetSimulation();
 TestEqual(TEXT("display LOD only"),A->RenderedPebbles(),8);TestEqual(TEXT("hidden display"),B->RenderedPebbles(),0);TestEqual(TEXT("all 32 particles still simulated"),B->TotalPebbles,32);
 A->ToggleSimulation();B->ToggleSimulation();const double Start=FPlatformTime::Seconds();
 while(FPlatformTime::Seconds()-Start<10&&(A->bRunning||B->bRunning)){A->Tick(.001f);B->Tick(1.f);FPlatformProcess::Sleep(.001f);}
 TestTrue(TEXT("workers finished cleanly"),A->bMixingFinished&&B->bMixingFinished&&A->Error.IsEmpty()&&B->Error.IsEmpty());
 TestEqual(TEXT("same step count independent of delta and LOD"),A->SnapshotSteps,B->SnapshotSteps);
 for(int I=0;I<A->BodyCount();++I)TestTrue(TEXT("display LOD never alters physics"),A->BodyPositionCm(I).Equals(B->BodyPositionCm(I),0));
 TestEqual(TEXT("measurements not driven by render frames"),A->MixingSampleCount(),11);
 TestTrue(TEXT("bounded snapshots not every physics step"),A->ProfiledDisplayUpdates<100);
 const int32 Updates=A->ProfiledDisplayUpdates;for(int I=0;I<100;++I)A->Tick(.1f);TestEqual(TEXT("unchanged snapshot skips render update"),A->ProfiledDisplayUpdates,Updates);
 TestTrue(TEXT("worker export after finish"),A->ExportState()&&B->ExportState());FString AT,BT;
 FFileHelper::LoadFileToString(AT,*(A->ExportDirectory/TEXT("bodies.csv")));FFileHelper::LoadFileToString(BT,*(B->ExportDirectory/TEXT("bodies.csv")));TestEqual(TEXT("all bodies export identical with display disabled"),AT,BT);
 AddInfo(FString::Printf(TEXT("OPT06_UNREAL_CPU: 2000 steps / 32 bodies; LOD8 worker %.9g s snapshot %.9g s display %.9g s (%d updates); hidden worker %.9g s snapshot %.9g s display %.9g s (%d updates). NullRHI excludes GPU drawing; concurrent workers are not speedup evidence."),A->WorkerComputeSeconds,A->SnapshotCopySeconds,A->ProfiledDisplaySeconds,A->ProfiledDisplayUpdates,B->WorkerComputeSeconds,B->SnapshotCopySeconds,B->ProfiledDisplaySeconds,B->ProfiledDisplayUpdates));
 IFileManager::Get().DeleteDirectory(*A->ExportDirectory,false,true);IFileManager::Get().DeleteDirectory(*B->ExportDirectory,false,true);
 const int64 Gen=A->SnapshotGeneration;A->ToggleSimulation();A->ResetSimulation();TestTrue(TEXT("reset retires old generation"),A->SnapshotGeneration>Gen&&A->SnapshotSteps==0&&A->PhysicalSeconds==0);
 A->ToggleSimulation();FPlatformProcess::Sleep(.005f);A->ResetSimulation();A->Tick(1);TestEqual(TEXT("running reset cannot publish old time"),A->PhysicalSeconds,0.);
 A->ToggleSimulation();A->Destroy();B->Destroy();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDEMCPUParallel,"Powder.DEM.CPUParallel",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDEMCPUParallel::RunTest(const FString& Parameters){
 UWorld::InitializationValues IV;IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);if(!TestNotNull(TEXT("CPU parallel world"),W))return false;
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());
 auto* A=W->SpawnActor<APowderClumpTest>();auto* B=W->SpawnActor<APowderClumpTest>();A->DispatchBeginPlay();B->DispatchBeginPlay();
 A->CPUThreads=4;A->ParallelMinJobs=1;A->bMixingRig=B->bMixingRig=true;A->ResetSimulation();B->ResetSimulation();
 for(int I=0;I<200;++I){A->StepSimulation();B->StepSimulation();}
 TestTrue(TEXT("parallel worker no errors"),A->Error.IsEmpty()&&B->Error.IsEmpty());TestEqual(TEXT("parallel integration applied"),A->LastIntegrationThreads,4);
 for(int I=0;I<A->BodyCount();++I)TestTrue(TEXT("parallel exact position parity"),A->BodyPositionCm(I).Equals(B->BodyPositionCm(I),0));
 TestEqual(TEXT("physical clock parity"),A->PhysicalSeconds,B->PhysicalSeconds);
 A->CPUThreads=2;A->StepSimulation();TestEqual(TEXT("CPU edits require reset"),A->LastIntegrationThreads,4);A->ResetSimulation();A->StepSimulation();TestEqual(TEXT("reset applies CPU threads"),A->LastIntegrationThreads,2);
 TestTrue(TEXT("parallel export"),A->ExportState());FString Text;FFileHelper::LoadFileToString(Text,*(A->ExportDirectory/TEXT("configuration.txt")));TestTrue(TEXT("export records CPU settings"),Text.Contains(TEXT("cpu_threads 2")));if(!A->ExportDirectory.IsEmpty())IFileManager::Get().DeleteDirectory(*A->ExportDirectory,false,true);
 A->CPUThreads=0;A->ResetSimulation();TestFalse(TEXT("invalid CPU setting rejected"),A->Error.IsEmpty());TestEqual(TEXT("invalid reset clears particles"),A->RenderedPebbles(),0);
 A->CPUThreads=4;A->ParallelMinJobs=10000;A->ResetSimulation();A->StepSimulation();TestEqual(TEXT("small workload serial fallback"),A->LastIntegrationThreads,1);
 A->Destroy();B->Destroy();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDEMGpuBridge,"Powder.DEM.GpuBridge",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDEMGpuBridge::RunTest(const FString& Parameters){
 UWorld::InitializationValues IV;IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);if(!TestNotNull(TEXT("GPU bridge world"),W))return false;
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());auto* A=W->SpawnActor<APowderClumpTest>();A->DispatchBeginPlay();
 A->bMixingRig=true;A->bPreferGPU=true;A->bUniformDampingFriction=true;A->PowderStiffness=A->MixedStiffness=A->InclusionStiffness=10000;A->FixedDt=2e-6;A->MixingDurationSeconds=.0002;A->MixingSampleSeconds=.00002;A->bProfilePerformance=true;A->bRealtimePacing=false;A->ResetSimulation();
 AddInfo(A->BackendStatus);TestTrue(TEXT("GPU enabled for supported scene"),A->BackendStatus.StartsWith(TEXT("GPU")));TestTrue(TEXT("GPU reset valid"),A->Error.IsEmpty());
 for(int I=0;I<10;++I)A->StepSimulation();TestTrue(TEXT("GPU physical time advances"),FMath::Abs(A->PhysicalSeconds-.00002)<1e-12);TestEqual(TEXT("GPU renders all spheres"),A->RenderedPebbles(),32);TestEqual(TEXT("physical mixing sample"),A->MixingSampleCount(),2);
 A->ToggleSimulation();const double End=FPlatformTime::Seconds()+15;while(!A->bMixingFinished&&A->Error.IsEmpty()&&FPlatformTime::Seconds()<End){A->Tick(1.f/60);FPlatformProcess::Sleep(.001f);}TestTrue(TEXT("GPU mixing finishes"),A->bMixingFinished);TestTrue(TEXT("GPU runtime valid"),A->Error.IsEmpty());
 AddInfo(FString::Printf(TEXT("GPU bridge worker %.9fs snapshot %.9fs ISM update %.9fs (%d updates); physical %.9fs"),A->WorkerComputeSeconds,A->SnapshotCopySeconds,A->ProfiledDisplaySeconds,A->ProfiledDisplayUpdates,A->PhysicalSeconds));
 TArray<FVector> GpuPositions;for(int32 I=0;I<32;++I)GpuPositions.Add(A->BodyPositionCm(I));const double GpuMass=A->ChargeMassKg,GpuMix=A->MixingIndex;
 A->bPreferGPU=false;A->ResetSimulation();for(int I=0;I<10;++I)A->StepSimulation();A->ToggleSimulation();const double CpuEnd=FPlatformTime::Seconds()+15;while(!A->bMixingFinished&&A->Error.IsEmpty()&&FPlatformTime::Seconds()<CpuEnd){A->Tick(1.f/60);FPlatformProcess::Sleep(.001f);}TestTrue(TEXT("CPU comparison finishes"),A->bMixingFinished);for(int32 I=0;I<32;++I)TestTrue(TEXT("CPU/GPU rendered trajectories"),GpuPositions[I].Equals(A->BodyPositionCm(I),1e-5));TestEqual(TEXT("CPU/GPU mass"),A->ChargeMassKg,GpuMass);TestEqual(TEXT("CPU/GPU mixing"),A->MixingIndex,GpuMix);
 AddInfo(FString::Printf(TEXT("CPU bridge worker %.9fs snapshot %.9fs ISM update %.9fs (%d updates); physical %.9fs"),A->WorkerComputeSeconds,A->SnapshotCopySeconds,A->ProfiledDisplaySeconds,A->ProfiledDisplayUpdates,A->PhysicalSeconds));
 A->bPreferGPU=true;
 A->MixedStiffness=14000;A->ResetSimulation();TestTrue(TEXT("unsupported coefficients explicit CPU fallback"),A->BackendStatus.StartsWith(TEXT("CPU fallback:")));TestTrue(TEXT("fallback preserves runnable scene"),A->Error.IsEmpty());A->StepSimulation();TestTrue(TEXT("CPU fallback advances"),A->PhysicalSeconds>0);
 A->Destroy();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
#endif
