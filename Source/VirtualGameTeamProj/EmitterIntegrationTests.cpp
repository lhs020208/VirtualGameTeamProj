#include "PowderGranularMixer.h"
#include "DEMEmitter.h"
#include "DEMParticleSettings.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEmitterGPUBridge,"Powder.EmitterIntegration.GPUBridge",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEmitterGPUBridge::RunTest(const FString&){
 UWorld::InitializationValues IV;IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);
 if(!TestNotNull(TEXT("world"),W))return false;GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* M=W->SpawnActor<APowderGranularMixer>();M->UseEmitters=true;M->Running=false;M->AutomaticSequence=false;M->RPM=0;
 M->SetActorLocation(FVector(1000,2000,300));M->SetActorRotation(FRotator(0,90,0));
 auto* S=NewObject<UDEMParticleSettings>(M);S->bGPUApproximation=true;S->RadiusMeters=.006;S->DensityKgPerCubicMeter=1200;S->SphereMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/DEM/SM_DEMSphere"));
 for(int I=0;I<2;++I){auto* E=W->SpawnActor<ADEMEmitter>();E->ParticleSettings=S;E->ParticleCount=1;E->ColumnsX=E->ColumnsY=1;E->GPUGroup=I;E->SetActorLocation(M->GetActorTransform().TransformPosition(FVector(I? -30:30,20,30)));E->SetActorRotation(FRotator(0,180,0));E->InitialLinearVelocity=FVector(.2,0,0);M->Emitters.Add(E);}
 std::vector<powder::Grain> P,V;
 TestTrue(TEXT("collect"),M->BuildEmitterInitialState(P,V));TestEqual(TEXT("count"),int(P.size()),2);
 if(P.size()==2){TestTrue(TEXT("translated/yawed local position"),FMath::Abs(P[0].x-.3)<1e-6&&FMath::Abs(P[0].y-.2)<1e-6&&FMath::Abs(P[0].z-.306)<1e-6);TestTrue(TEXT("rotated velocity"),FMath::Abs(V[0].x)<1e-6&&FMath::Abs(V[0].y-.2)<1e-6);TestEqual(TEXT("red group"),P[0].w,0.f);TestEqual(TEXT("blue group"),P[1].w,1.f);}
 auto Duplicate=M->Emitters[0];M->Emitters.Add(Duplicate);TestFalse(TEXT("duplicate rejected"),M->BuildEmitterInitialState(P,V));M->Emitters.Pop();
 auto SavedPosition=M->Emitters[1]->GetActorLocation();M->Emitters[1]->SetActorLocation(M->Emitters[0]->GetActorLocation());TestFalse(TEXT("cross-source overlap rejected"),M->BuildEmitterInitialState(P,V));M->Emitters[1]->SetActorLocation(SavedPosition);
 auto* Other=DuplicateObject<UDEMParticleSettings>(S,M);Other->DensityKgPerCubicMeter=1000;M->Emitters[1]->ParticleSettings=Other;TestFalse(TEXT("mixed density rejected"),M->BuildEmitterInitialState(P,V));M->BinaryMaterials=true;Other->RadiusMeters=.005;Other->WallFriction=.2;TestTrue(TEXT("binary material bridge"),M->BuildEmitterInitialState(P,V));TestEqual(TEXT("binary density snapshot"),M->EmitterConfig.materials[1].density_kg_m3,1000.);TestEqual(TEXT("binary wall friction"),M->EmitterConfig.materials[1].wall_friction,.2);M->BinaryMaterials=false;M->Emitters[1]->ParticleSettings=S;
 M->SetActorRotation(FRotator(5,90,0));TestFalse(TEXT("tilted mixer rejected"),M->BuildEmitterInitialState(P,V));M->SetActorRotation(FRotator(0,90,0));
 W->InitializeActorsForPlay(FURL());for(auto E:M->Emitters)E->DispatchBeginPlay();M->DispatchBeginPlay();
 auto Wait=[&](auto Pred){double End=FPlatformTime::Seconds()+15;while(FPlatformTime::Seconds()<End){M->Tick(.016f);if(Pred())return true;FPlatformProcess::Sleep(.002f);}AddError(TEXT("worker timeout"));return false;};
 Wait([&]{return M->CopyParticleState().size()==2||!M->Error.IsEmpty();});TestTrue(TEXT("worker ready"),M->Error.IsEmpty());
 auto Initial=M->CopyParticleState();TestEqual(TEXT("paused initial time"),M->PhysicalSeconds,0.);
 TestTrue(TEXT("GPU source owned"),M->Emitters[0]->GetGPUOwner()==M);TestEqual(TEXT("one instance, no duplicate"),M->Emitters[0]->GetParticleInstances()->GetInstanceCount(),1);
 M->Running=true;Wait([&]{return M->PhysicalSeconds>=.03||!M->Error.IsEmpty();});M->Running=false;M->Tick(.016f);FPlatformProcess::Sleep(.1f);M->Tick(.016f);
 auto Final=M->CopyParticleState();double T=M->PhysicalSeconds;
 if(Initial.size()==2&&Final.size()==2){TestTrue(TEXT("initial velocity integrated"),FMath::Abs(Final[0].y-(Initial[0].y+.2*T))<.0002);TestTrue(TEXT("gravity integrated"),FMath::Abs(Final[0].z-(Initial[0].z-9.81*T*(T+M->TimeStep)/2))<.0002);TestEqual(TEXT("group preserved"),Final[1].w,1.f);}
 M->ResetSimulation();Wait([&]{return M->CopyParticleState().size()==2;});TestEqual(TEXT("reset time"),M->PhysicalSeconds,0.);

 TestFalse(TEXT("standalone regeneration refused"),M->Emitters[0]->TryGenerate());M->Running=true;M->Tick(.016f);TestFalse(TEXT("invalidated source pauses"),M->Running);TestTrue(TEXT("reset requested"),M->Error.Contains(TEXT("Reset")));
 M->ResetSimulation();Wait([&]{return M->CopyParticleState().size()==2;});TestTrue(TEXT("reset recovers"),M->Error.IsEmpty());
 for(int I=0;I<3;++I){M->ResetSimulation();Wait([&]{return M->CopyParticleState().size()==2;});TestEqual(TEXT("repeated reset count"),M->Emitters[0]->GetParticleInstances()->GetInstanceCount(),1);}
 M->Emitters[1]->Destroy();M->Running=true;M->Tick(.016f);TestFalse(TEXT("deleted source pauses"),M->Running);
 M->Destroy();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
#endif

