#include "PowderBulkPrototype.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HAL/PlatformTime.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBulkPrototypeBridge,"Powder.DEM.BulkPrototype",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBulkPrototypeBridge::RunTest(const FString& Parameters){
 UWorld::InitializationValues IV;IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);if(!TestNotNull(TEXT("world"),W))return false;GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* A=W->SpawnActor<APowderBulkPrototype>();A->SetActorRotation(FRotator(-20,0,0));A->ResetSimulation();TestTrue(TEXT("configured"),A->Error.IsEmpty());A->ValveOpen=false;A->TargetRate=1;
 TArray<UInstancedStaticMeshComponent*> Groups;A->GetComponents(Groups);int32 Count=0;for(auto* M:Groups)Count+=M->GetInstanceCount();TestEqual(TEXT("visible tracers"),Count,1500);
 double Start=FPlatformTime::Seconds();for(int32 I=0;I<1800;++I)A->Tick(1.f/60);double Wall=FPlatformTime::Seconds()-Start;
 TestTrue(TEXT("closed valve tick valid"),A->Error.IsEmpty());TestTrue(TEXT("physical clock advanced"),A->PhysicalSeconds>29);
 for(auto* M:Groups)for(int32 I=0;I<M->GetInstanceCount();++I){FTransform T;M->GetInstanceTransform(I,T,false);auto P=T.GetLocation();if(P.X>198.001||P.X<.999||FMath::Abs(P.Y)>19.2||P.Z< -19.2||P.Z>19.2){AddError(TEXT("visible tracer escaped supported wall or closed gate"));break;}}
 double Paused=A->PhysicalSeconds;A->Running=false;A->Tick(1);TestEqual(TEXT("pause"),A->PhysicalSeconds,Paused);A->Running=true;A->ValveOpen=true;for(int32 I=0;I<600;++I)A->Tick(1.f/60);bool Crossed=false;for(auto* M:Groups)for(int32 I=0;I<M->GetInstanceCount();++I){FTransform T;M->GetInstanceTransform(I,T,false);Crossed|=T.GetLocation().X>202;}TestTrue(TEXT("opening changes visible flow"),Crossed);
 AddInfo(FString::Printf(TEXT("Bulk bridge 30 physical seconds with instance updates: wall %.6f s; no GPU frame timing claim"),Wall));A->ResetSimulation();TestEqual(TEXT("reset"),A->PhysicalSeconds,0.);A->Destroy();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
#endif
