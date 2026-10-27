#include "DEMCommonCommandlet.h"
#include "DEMWorld.h"
#include "DEMEmitter.h"
#include "DEMPlaneBoundary.h"
#include "DEMCoordinates.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/SavePackage.h"
#include "FileHelpers.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "StaticMeshCompiler.h"
#include "Math/RotationMatrix.h"
#include "EngineUtils.h"
namespace
{
const TCHAR* WorldPath=TEXT("/Game/DEM/Simulation/BP_DEMWorld");
const TCHAR* FacePath=TEXT("/Game/DEM/Boundaries/BP_DEMPlaneBoundary");
const TCHAR* IntegrationPath=TEXT("/Game/DEM/DEM_Integration");
UBlueprint* Blueprint(const TCHAR* Path,UClass* Parent,bool Create)
{
 const FString Name=FPackageName::GetLongPackageAssetName(Path);
 if(FPackageName::DoesPackageExist(Path))
 {
  UBlueprint* B=LoadObject<UBlueprint>(nullptr,*(FString(Path)+TEXT(".")+Name));
  UE_LOG(LogTemp,Display,TEXT("Existing asset preserved: %s"),Path);
  return B&&B->GeneratedClass&&B->ParentClass==Parent?B:nullptr;
 }
 if(!Create)return nullptr;
 UPackage* P=CreatePackage(Path);
 UBlueprint* B=FKismetEditorUtilities::CreateBlueprint(Parent,P,FName(*Name),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass(),TEXT("CommonDEM"));
 if(!B)return nullptr;FKismetEditorUtilities::CompileBlueprint(B);if(B->Status==BS_Error)return nullptr;
 const FString File=FPackageName::LongPackageNameToFilename(Path,FPackageName::GetAssetPackageExtension());
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(File),true);FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;
 return UPackage::SavePackage(P,B,*File,A)?B:nullptr;
}
UWorld* TestWorld()
{
 UWorld::InitializationValues I;I.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).CreateFXSystem(false);
 return UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::SM5,&I);
}
void Begin(UWorld* W){W->InitializeActorsForPlay(FURL());W->GetWorldSettings()->NotifyBeginPlay();}
void Destroy(UWorld* W){W->EndPlay(EEndPlayReason::Quit);W->DestroyWorld(false);}
ADEMEmitter* Emitter(UWorld* W,UClass* Class,UDEMParticleSettings* S,int32 Order,FVector Position,int32 Count=1)
{
 auto* E=W->SpawnActor<ADEMEmitter>(Class);E->ParticleSettings=S;E->GenerationOrder=Order;E->ParticleCount=Count;E->ColumnsX=Count==1?1:10;E->ColumnsY=Count==1?1:10;
 E->SetActorLocation(Position);return E;
}
ADEMPlaneBoundary* Face(UWorld* W,UClass* C,int32 ID,FVector Position,FRotator Rotation,double Width,double Height,bool Support=false)
{
 auto* A=W->SpawnActor<ADEMPlaneBoundary>(C);A->Boundary->BoundaryID=ID;A->Boundary->Width=Width;A->Boundary->Height=Height;A->Boundary->bMeasureSupport=Support;
 A->SetActorLocationAndRotation(Position,Rotation);A->OnConstruction(A->GetActorTransform());return A;
}
bool SameState(const std::vector<dem::Particle>&A,const std::vector<dem::Particle>&B)
{
 if(A.size()!=B.size())return false;for(size_t I=0;I<A.size();++I)if(A[I].id!=B[I].id||(A[I].p-B[I].p).norm2()!=0||(A[I].v-B[I].v).norm2()!=0||(A[I].w-B[I].w).norm2()!=0)return false;return true;
}
}
UDEMCommonCommandlet::UDEMCommonCommandlet(){IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;}
int32 UDEMCommonCommandlet::Main(const FString& Params)
{
 const bool Create=FParse::Param(*Params,TEXT("CreateAssets"));
 auto* User=LoadObject<UDEMParticleSettings>(nullptr,TEXT("/Game/DEM/Settings/DA_DEMParticle_Default.DA_DEMParticle_Default"));
 auto* EBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/DEM/Emitters/BP_DEMEmitter.BP_DEMEmitter"));
 if(!User||!EBP||!EBP->GeneratedClass)return 1;
 FStaticMeshCompilingManager::Get().FinishAllCompilation();
 UE_LOG(LogTemp,Display,TEXT("Current user settings preserved: r=%.17g rho=%.17g kn=%.17g kt=%.17g zn=%.17g zt=%.17g mu=%.17g muWall=%.17g"),User->RadiusMeters,User->DensityKgPerCubicMeter,User->NormalStiffness,User->TangentialStiffness,User->NormalDampingRatio,User->TangentialDampingRatio,User->ParticleFriction,User->WallFriction);
 auto* WBP=Blueprint(WorldPath,ADEMWorld::StaticClass(),Create);auto* FBP=Blueprint(FacePath,ADEMPlaneBoundary::StaticClass(),Create);
 if(!WBP||!FBP)return 2;
 // Isolated settings instance for numeric assumptions, never edit/save User.
 auto* Settings=DuplicateObject<UDEMParticleSettings>(User,GetTransientPackage());
 Settings->RadiusMeters=.01;Settings->DensityKgPerCubicMeter=1000;Settings->NormalStiffness=500000;Settings->TangentialStiffness=250000;
 Settings->NormalDampingRatio=Settings->TangentialDampingRatio=.2;Settings->ParticleFriction=Settings->WallFriction=.4;
 Settings->SphereMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/DEM/SM_DEMSphere.SM_DEMSphere"));
 FDEMParticleRenderSnapshot Snapshot;TArray<FText> Errors;if(!Settings->TryCreateRenderSnapshot(Snapshot,Errors))return 3;
 if(Create)
 {
  if(FPackageName::DoesPackageExist(IntegrationPath))
  {auto* Existing=LoadObject<UWorld>(nullptr,TEXT("/Game/DEM/DEM_Integration.DEM_Integration"));UE_LOG(LogTemp,Display,TEXT("Existing integration map inspected, not overwritten."));return Existing?0:4;}
  const bool Standard=User->RadiusMeters==.01&&User->DensityKgPerCubicMeter==1000&&User->NormalStiffness==500000&&User->TangentialStiffness==250000&&
  User->NormalDampingRatio==.2&&User->TangentialDampingRatio==.2&&User->ParticleFriction==.4&&User->WallFriction==.4&&User->SphereMesh==Settings->SphereMesh;
  UDEMParticleSettings* MapSettings=User;
  if(!Standard)
  {
   const TCHAR* PName=TEXT("/Game/DEM/Settings/DA_DEMParticle_IntegrationTest");
   if(FPackageName::DoesPackageExist(PName)) {MapSettings=LoadObject<UDEMParticleSettings>(nullptr,TEXT("/Game/DEM/Settings/DA_DEMParticle_IntegrationTest.DA_DEMParticle_IntegrationTest"));if(!MapSettings)return 5;}
   else
   {UPackage* P=CreatePackage(PName);MapSettings=DuplicateObject<UDEMParticleSettings>(Settings,P,TEXT("DA_DEMParticle_IntegrationTest"));MapSettings->SetFlags(RF_Public|RF_Standalone);
    FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;if(!UPackage::SavePackage(P,MapSettings,*FPackageName::LongPackageNameToFilename(PName,FPackageName::GetAssetPackageExtension()),A))return 6;}
  }
  UWorld* W=GEditor->NewMap();auto* Manager=W->SpawnActor<ADEMWorld>(WBP->GeneratedClass.Get());
  auto* E=Emitter(W,EBP->GeneratedClass.Get(),MapSettings,0,FVector(0,0,29),200);E->TargetSimulation=Manager;E->RefreshPreview();
  Face(W,FBP->GeneratedClass.Get(),0,FVector::ZeroVector,FRotator::ZeroRotator,.3,.3,true)->Boundary->TargetSimulation=Manager;
  Face(W,FBP->GeneratedClass.Get(),1,FVector(-15,0,12.5),FRotator(0,0,90),.3,.25)->SetActorRotation(FRotator(0,90,90));
  Face(W,FBP->GeneratedClass.Get(),2,FVector(15,0,12.5),FRotator(0,90,90),.3,.25);
  Face(W,FBP->GeneratedClass.Get(),3,FVector(0,-15,12.5),FRotator(0,0,90),.3,.25);
  Face(W,FBP->GeneratedClass.Get(),4,FVector(0,15,12.5),FRotator(0,0,90),.3,.25);
  auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
  auto* Floor=W->SpawnActor<AStaticMeshActor>(FVector(0,0,-.25),FRotator::ZeroRotator);Floor->SetActorLabel(TEXT("Visual floor only - DEM uses registered finite face"));
  Floor->GetStaticMeshComponent()->SetStaticMesh(Cube);Floor->SetActorScale3D(FVector(.32,.32,.005));Floor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);Floor->GetStaticMeshComponent()->SetCanEverAffectNavigation(false);
  auto* Light=W->SpawnActor<ADirectionalLight>(FVector(0,0,100),FRotator(-55,-35,0));Light->GetLightComponent()->SetIntensity(8);
  W->SpawnActor<ASkyLight>()->GetLightComponent()->SetIntensity(1);
  const FVector CameraPosition(55,-65,65);auto* Camera=W->SpawnActor<ACameraActor>(CameraPosition,(FVector(0,0,10)-CameraPosition).Rotation());Camera->GetCameraComponent()->FieldOfView=45;
  if(!UEditorLoadingAndSavingUtils::SaveMap(W,IntegrationPath))return 7;
  UE_LOG(LogTemp,Display,TEXT("Created original-project common DEM Blueprints and DEM_Integration; existing maps/startup unchanged."));return 0;
 }
 int Checks=0,Failures=0;FString Report=TEXT("test,status\n");
 auto Check=[&](bool OK,const TCHAR* Name){++Checks;if(!OK)++Failures;Report+=FString::Printf(TEXT("%s,%s\n"),Name,OK?TEXT("PASS"):TEXT("FAIL"));UE_LOG(LogTemp,Display,TEXT("%s %s"),OK?TEXT("PASS"):TEXT("FAIL"),Name);};
 auto* Map=LoadObject<UWorld>(nullptr,TEXT("/Game/DEM/DEM_Integration.DEM_Integration"));
 Check(Map&&WBP->GeneratedClass->IsChildOf(ADEMWorld::StaticClass())&&FBP->GeneratedClass->IsChildOf(ADEMPlaneBoundary::StaticClass()),TEXT("original_project_blueprint_map_reload"));
 const FVector V(1,2,3);const FQuat Q=FRotator(20,30,40).Quaternion();
 Check(DEMCoordinates::PositionToUE(DEMCoordinates::PositionToDEM(V)).Equals(V,1e-12)&&DEMCoordinates::Polar(DEMCoordinates::Polar(V))==V&&
 DEMCoordinates::Axial(DEMCoordinates::Axial(V))==V&&DEMCoordinates::Orientation(DEMCoordinates::Orientation(Q)).Equals(Q,1e-12),TEXT("coordinate_roundtrip_position_polar_axial_quaternion"));
 UWorld* W=TestWorld();auto* E2=Emitter(W,EBP->GeneratedClass.Get(),Settings,20,FVector(1.1,0,100));E2->InitialLinearVelocity=FVector(-1,0,0);
 auto* E1=Emitter(W,EBP->GeneratedClass.Get(),Settings,10,FVector(-1.1,0,100));E1->InitialLinearVelocity=FVector(1,0,0);
 auto* F=Face(W,FBP->GeneratedClass.Get(),0,FVector::ZeroVector,FRotator::ZeroRotator,.3,.3,true);
 auto* M=W->SpawnActor<ADEMWorld>(WBP->GeneratedClass.Get());M->Gravity=FVector::ZeroVector;M->bAutoStart=false;M->MaxSubsteps=10000;
 Begin(W);M->Advance(0);Check(M->IsReady()&&M->ParticleCount==2&&E1->FirstGlobalID==0&&E2->FirstGlobalID==1,TEXT("beginplay_order_and_explicit_stable_id_order"));
 M->RegisterEmitter(E1);M->RegisterEmitter(E1);M->RegisterBoundary(F->Boundary);M->RegisterBoundary(F->Boundary);
 Check(!M->bResetRequired&&M->EmitterCount==2&&M->BoundaryCount==1,TEXT("idempotent_registration"));
 const auto Initial=M->GetSolver().p;M->Start();M->Advance(.03);
 Check(M->GetSolver().s.steps>=2999&&M->GetSolver().p[0].v.x<0&&M->GetSolver().p[1].v.x>0,TEXT("two_emitters_collide_in_one_solver"));
 Check((M->GetSolver().p[0].v*M->GetSolver().p[0].m+M->GetSolver().p[1].v*M->GetSolver().p[1].m).norm()<1e-12,TEXT("shared_collision_momentum"));
 M->Reset();Check(SameState(Initial,M->GetSolver().p)&&M->SimulationTime==0&&M->GetSolver().history.empty(),TEXT("reset_initial_state_time_history"));
 M->Step();Check(M->GetSolver().s.steps==1&&!M->bRunning&&M->SimulationTime==M->FixedDt,TEXT("step_exactly_one_dt"));
 M->Reset();M->DisplayHz=10;M->Start();M->Advance(.01);const auto Slow=M->GetSolver().p;const auto Steps=M->GetSolver().s.steps;
 M->Reset();M->DisplayHz=240;M->Start();M->Advance(.01);Check(M->GetSolver().s.steps==Steps&&SameState(Slow,M->GetSolver().p),TEXT("display_frequency_physics_invariance"));
 const auto BeforeMove=M->GetSolver().p;E1->SetActorLocation(FVector(900,0,0));M->Pause();M->Advance(0);
 Check(!M->bResetRequired&&SameState(BeforeMove,M->GetSolver().p),TEXT("moving_emitter_does_not_teleport_registered_particles"));E1->SetActorLocation(FVector(-1.1,0,100));
 E1->Generate();Check(M->bResetRequired&&!M->bRunning,TEXT("connected_generate_requires_common_reset"));M->Reset();
 F->SetActorLocation(FVector(0,0,1));M->Advance(0);Check(M->bResetRequired&&!M->bRunning,TEXT("moving_fixed_boundary_stops"));F->SetActorLocation(FVector::ZeroVector);M->Reset();
 E2->GenerationOrder=10;M->Reset();Check(M->bResetRequired,TEXT("duplicate_generation_order_rejected"));E2->GenerationOrder=20;M->Reset();
 auto* Other=DuplicateObject<UDEMParticleSettings>(Settings,GetTransientPackage());Other->DensityKgPerCubicMeter=1200;E2->ParticleSettings=Other;M->Reset();
 Check(M->bResetRequired,TEXT("mixed_physics_rejected_not_overwritten"));E2->ParticleSettings=Settings;M->Reset();
 auto* Late=Emitter(W,EBP->GeneratedClass.Get(),Settings,30,FVector(0,0,200));Check(M->bResetRequired,TEXT("late_registration_stops"));Late->Destroy();M->Reset();
 auto* Extra=W->SpawnActor<ADEMWorld>();M->Advance(0);FString Why;Check(M->bResetRequired&&ADEMWorld::Resolve(W,M,Why)==nullptr,TEXT("multiple_managers_ambiguous_even_with_explicit_target"));Extra->Destroy();M->Reset();
 M->MaximumLag=.001;M->Start();const uint64 BeforeLag=M->GetSolver().s.steps;M->Advance(.01);
 Check(!M->bRunning&&M->Lag>=.01&&M->GetSolver().s.steps==BeforeLag,TEXT("lag_limit_pauses_without_skipping_time"));M->MaximumLag=2;M->Reset();
 Check(!M->SetExternalLoad(-1,FVector::ZeroVector,FVector::ZeroVector)&&M->bResetRequired,TEXT("manager_invalid_external_id"));M->Reset();
 E1->Destroy();M->Advance(0);Check(M->bResetRequired,TEXT("registered_emitter_deletion_stops"));Destroy(W);
 UWorld* Alone=TestWorld();auto* Solo=Emitter(Alone,EBP->GeneratedClass.Get(),Settings,0,FVector(0,0,20));Begin(Alone);FDEMEmitterBurst SoloData;
 Check(ADEMWorld::Resolve(Alone,nullptr,Why)==nullptr&&Solo->CopyGeneratedBurst(SoloData)&&SoloData.Particles.Num()==1,TEXT("missing_manager_keeps_explicit_standalone_static_mode"));Destroy(Alone);
 // 5 seconds through the real manager / emitter / boundary path (no saved level edits).
 UWorld* D=TestWorld();auto* DM=D->SpawnActor<ADEMWorld>(WBP->GeneratedClass.Get());DM->MaxSubsteps=2000;
 auto* DE=Emitter(D,EBP->GeneratedClass.Get(),Settings,0,FVector(0,0,29),200);
 Face(D,FBP->GeneratedClass.Get(),0,FVector::ZeroVector,FRotator::ZeroRotator,.3,.3,true);
 Face(D,FBP->GeneratedClass.Get(),1,FVector(-15,0,12.5),FRotator(0,90,90),.3,.25);
 Face(D,FBP->GeneratedClass.Get(),2,FVector(15,0,12.5),FRotator(0,90,90),.3,.25);
 Face(D,FBP->GeneratedClass.Get(),3,FVector(0,-15,12.5),FRotator(0,0,90),.3,.25);
 Face(D,FBP->GeneratedClass.Get(),4,FVector(0,15,12.5),FRotator(0,0,90),.3,.25);
 Begin(D);DM->Advance(0);
 double MaxOverlap=0,SumKE=0,SumSupport=0;int Samples=0;uint64 Escapes=0;
 FString Trace=TEXT("time,ke,support,height,overlap,contacts,lag\n");
 for(int I=0;I<500&&!DM->bResetRequired;++I)
 {
  ++GFrameCounter;DM->Advance(.01);const auto&S=DM->GetSolver();MaxOverlap=FMath::Max(MaxOverlap,S.s.maxOverlap);
  if(S.s.time>=4){SumKE+=S.s.ke+S.s.rotKE;SumSupport+=S.s.support;++Samples;}
  for(const auto&P:S.p)if(P.p.z<0||std::abs(P.p.x)>.15+P.r||std::abs(P.p.y)>.15+P.r)++Escapes;
  Trace+=FString::Printf(TEXT("%.17g,%.17g,%.17g,%.17g,%.17g,%llu,%.17g\n"),S.s.time,S.s.ke+S.s.rotKE,S.s.support,S.s.height,S.s.maxOverlap,uint64(S.s.contacts),DM->Lag);
 }
 for(int Catchup=0;Catchup<2000&&!DM->bResetRequired&&DM->GetSolver().s.steps<500000;++Catchup)DM->Step();
 const auto& S=DM->GetSolver();
 Check(!DM->bResetRequired&&!S.failed&&S.p.size()==200&&S.s.steps==500000&&S.boundaryCrossings==0&&Escapes==0,TEXT("manager_200_particles_5seconds_finite_no_crossings_escape"));
 Check(MaxOverlap<.01&&SumKE/FMath::Max(1,Samples)<1e-5&&FMath::Abs(SumSupport/FMath::Max(1,Samples)-S.s.mass*9.81)/(S.s.mass*9.81)<.1,TEXT("manager_deposition_overlap_energy_support"));
 FTransform Rendered;DE->GetParticleInstances()->GetInstanceTransform(0,Rendered,true);
 Check(Rendered.GetLocation().Equals(DEMCoordinates::PositionToUE(S.p[0].p),.001),TEXT("ISM_tracks_common_solver_without_duplicate_instances"));
 const FString Out=FPaths::ProjectPluginsDir()/TEXT("DEMPrototype/CommonValidation");
 FFileHelper::SaveStringToFile(Trace,*(Out/TEXT("manager_deposit_trace.csv")));
 const FString Metrics=FString::Printf(TEXT("{\"steps\":%llu,\"time\":%.17g,\"particles\":%d,\"mass\":%.17g,\"sampled_max_overlap\":%.17g,\"settled_ke\":%.17g,\"support\":%.17g,\"crossings\":%llu,\"escape_samples\":%llu}"),uint64(S.s.steps),S.s.time,int32(S.p.size()),S.s.mass,MaxOverlap,SumKE/FMath::Max(1,Samples),SumSupport/FMath::Max(1,Samples),uint64(S.boundaryCrossings),Escapes);
 FFileHelper::SaveStringToFile(Metrics,*(Out/TEXT("manager_deposit.json")));Destroy(D);
 FFileHelper::SaveStringToFile(Report,*(Out/TEXT("engine_checks.csv")));
 FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"checks\":%d,\"failures\":%d}"),Checks,Failures),*(Out/TEXT("engine_summary.json")));
 return Failures?10:0;
}
