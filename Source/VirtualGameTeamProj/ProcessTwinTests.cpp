#include "PowderProcessTwin.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBulkProcessBridge,"Powder.DEM.BulkProcess",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBulkProcessBridge::RunTest(const FString& Parameters){
 UWorld::InitializationValues IV;IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);if(!TestNotNull(TEXT("world"),W))return false;GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());auto* A=W->SpawnActor<APowderProcessTwin>();A->TotalKg=10;A->TargetRate=10;A->DispatchBeginPlay();A->ResetSimulation();TestTrue(TEXT("valid config"),A->Error.IsEmpty());TestTrue(TEXT("visible powder"),A->InstanceCount()>1000);
 A->Running=false;double Before=A->PhysicalSeconds;FPlatformProcess::Sleep(.02f);A->Tick(.02f);TestEqual(TEXT("pause"),A->PhysicalSeconds,Before);A->Running=true;
 double End=FPlatformTime::Seconds()+15;while(!A->Complete()&&A->Error.IsEmpty()&&FPlatformTime::Seconds()<End){FPlatformProcess::Sleep(.002f);A->Tick(.002f);}
 TestTrue(TEXT("complete rendered instance sequence"),A->Complete());TestTrue(TEXT("no error"),A->Error.IsEmpty());TestTrue(TEXT("delivered mass"),A->MixerKg>9.9&&A->MixerKg<=10.000001);TestTrue(TEXT("mix index bounded"),A->MixingIndex>=0&&A->MixingIndex<=1);auto* State=A->StateForValidation();auto Mass=State->graph.totals();for(unsigned G=0;G<3;++G)TestTrue(TEXT("species conserved"),FMath::Abs(Mass[G]-State->initial[G])<1e-8);
 A->ResetSimulation();TestEqual(TEXT("reset phase clock"),A->PhysicalSeconds,0.);TestTrue(TEXT("reset restores initial mass"),A->StateForValidation()->graph.nodes[0].mass[0]>3);A->Destroy();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
#endif
