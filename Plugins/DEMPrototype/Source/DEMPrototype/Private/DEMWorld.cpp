#include "DEMWorld.h"
#include "DEMEmitter.h"
#include "DEMPlaneBoundary.h"
#include "DEMCoordinates.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include <cmath>
namespace
{
bool SamePhysics(const FDEMParticlePhysicsSnapshot&A,const FDEMParticlePhysicsSnapshot&B)
{
 return A.RadiusMeters==B.RadiusMeters&&A.DensityKgPerCubicMeter==B.DensityKgPerCubicMeter&&A.NormalStiffness==B.NormalStiffness&&
 A.TangentialStiffness==B.TangentialStiffness&&A.NormalDampingRatio==B.NormalDampingRatio&&A.TangentialDampingRatio==B.TangentialDampingRatio&&
 A.ParticleFriction==B.ParticleFriction&&A.WallFriction==B.WallFriction;
}
bool SameFace(const dem::Plane&A,const dem::Plane&B)
{
 return A.id==B.id && (A.center-B.center).norm2()<1e-20 && (A.u-B.u).norm2()<1e-20 && (A.v-B.v).norm2()<1e-20 &&
 A.halfU==B.halfU&&A.halfV==B.halfV&&A.measureSupport==B.measureSupport;
}
void Gather(UWorld* W,TArray<ADEMEmitter*>& Emitters,TArray<UDEMPlaneBoundaryComponent*>& Bounds)
{
 for(TActorIterator<ADEMEmitter> It(W);It;++It) if(IsValid(*It)) Emitters.Add(*It);
 for(TActorIterator<AActor> It(W);It;++It) if(IsValid(*It)) {TArray<UDEMPlaneBoundaryComponent*> C;It->GetComponents(C);for(auto* B:C)if(IsValid(B))Bounds.AddUnique(B);}
 Emitters.Sort([](const ADEMEmitter&A,const ADEMEmitter&B){return A.GenerationOrder<B.GenerationOrder;});
 Bounds.Sort([](const UDEMPlaneBoundaryComponent&A,const UDEMPlaneBoundaryComponent&B){return A.BoundaryID<B.BoundaryID;});
}
}
ADEMWorld::ADEMWorld(){PrimaryActorTick.bCanEverTick=true;SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("IgnoredTransform")));}
ADEMWorld* ADEMWorld::Resolve(UWorld* W,ADEMWorld* Explicit,FString& Error)
{
 TArray<ADEMWorld*> Managers; if(W)for(TActorIterator<ADEMWorld> It(W);It;++It)if(IsValid(*It))Managers.Add(*It);
 if(Managers.Num()!=1){Error=TEXT("Exactly one DEMWorld per world is supported; missing or ambiguous manager.");return nullptr;}
 if(Explicit && Explicit!=Managers[0]){Error=TEXT("Target Simulation does not match the sole manager in this world.");return nullptr;}
 return Managers[0];
}
void ADEMWorld::RegisterEmitter(ADEMEmitter* E)
{
 if(!E)return;
 if(bReady && !Sources.ContainsByPredicate([E](const Source&S){return S.E==E;})) Invalidate(TEXT("Late emitter registration; Reset required."));
 PendingEmitters.AddUnique(E);
}
void ADEMWorld::RegisterBoundary(UDEMPlaneBoundaryComponent* B)
{
 if(!B)return;
 if(bReady && !Faces.ContainsByPredicate([B](const FaceSource&S){return S.B==B;})) Invalidate(TEXT("Late boundary registration; Reset required."));
 PendingBoundaries.AddUnique(B);
}
void ADEMWorld::Invalidate(const FString& Why)
{
 if(!bResetRequired)UE_LOG(LogTemp,Warning,TEXT("DEMWorld: %s"),*Why);
 bRunning=false;bResetRequired=true;Status=Why;
}
bool ADEMWorld::CheckClock()
{
 if(!FMath::IsFinite(FixedDt)||FixedDt<=0||!FMath::IsFinite(SpeedMultiplier)||SpeedMultiplier<0||SpeedMultiplier>100||
 !FMath::IsFinite(MaximumLag)||MaximumLag<=0||MaxSubsteps<1||MaxSubsteps>10000||!FMath::IsFinite(DisplayHz)||DisplayHz<1||DisplayHz>240||Gravity.ContainsNaN())
 {Invalidate(TEXT("Invalid dt/gravity/speed/substeps/lag/display clock settings."));return false;}
 return true;
}
bool ADEMWorld::Initialize()
{
 if(!GetWorld()||!GetWorld()->IsGameWorld()||!GetWorld()->HasBegunPlay()){Status=TEXT("Waiting for all actors' BeginPlay; physics has not started.");return false;}
 if(!CheckClock())return false;
 FString Error;if(Resolve(GetWorld(),this,Error)!=this){Invalidate(Error);return false;}
 TArray<ADEMEmitter*> E;TArray<UDEMPlaneBoundaryComponent*> B;Gather(GetWorld(),E,B);
 if(E.IsEmpty()){Invalidate(TEXT("No emitter; place BP_DEMEmitter then Reset."));return false;}
 for(auto* X:E)if(!X->HasActorBegunPlay())return false;
 for(auto* X:B)if(!X->HasBegunPlay())return false;
 std::vector<dem::Particle> Initial;std::vector<dem::Plane> Boundaries;
 FDEMParticlePhysicsSnapshot Common;bool First=true;
 TArray<Source> NewSources;TArray<FaceSource> NewFaces;
 for(auto* X:E)
 {
  if(X->GenerationOrder<0 || (!NewSources.IsEmpty()&&NewSources.Last().Order==X->GenerationOrder))
  {Invalidate(TEXT("Emitter Generation Order must be nonnegative and unique. Reset after correction."));return false;}
  if(Resolve(GetWorld(),X->TargetSimulation,Error)!=this){Invalidate(Error);return false;}
  if(!X->bGenerateOnBeginPlay){Invalidate(TEXT("Connected emitters must enable Generate on BeginPlay; standalone disabled emitters are unsupported in a managed world."));return false;}
  X->ReleaseSimulation();
  if(!X->TryGenerate()){Invalidate(TEXT("Emitter generation failed: ")+X->GenerationStatus);return false;}
  FDEMEmitterBurst Burst;if(!X->CopyGeneratedBurst(Burst)){Invalidate(TEXT("Emitter has no valid burst."));return false;}
  if(First){Common=Burst.Settings;First=false;}else if(!SamePhysics(Common,Burst.Settings))
  {Invalidate(TEXT("Mixed physical settings unsupported: radius/density/kn/kt/damping/friction must match exactly. No overwrite performed."));return false;}
  if(Initial.size()+Burst.Particles.Num()>10000){Invalidate(TEXT("Common world limit is 10000 particles total."));return false;}
  Source S;S.E=X;S.First=int32(Initial.size());S.Count=Burst.Particles.Num();S.Order=X->GenerationOrder;NewSources.Add(S);
  for(const auto& P:Burst.Particles)
  {
   dem::Particle A;A.id=uint32(Initial.size());A.p=DEMCoordinates::Vector(P.PositionMeters);A.v=DEMCoordinates::Vector(P.LinearVelocityMetersPerSecond);
   A.w=DEMCoordinates::Vector(P.AngularVelocityRadiansPerSecond);A.q={P.Orientation.X,P.Orientation.Y,P.Orientation.Z,P.Orientation.W};
   A.r=Common.RadiusMeters;A.m=Common.Derived.MassKg;A.I=Common.Derived.InertiaKgMetersSquared;A.invM=Common.Derived.InverseMass;A.invI=Common.Derived.InverseInertia;Initial.push_back(A);
  }
 }
 if(Initial.empty()){Invalidate(TEXT("Common world needs at least one particle."));return false;}
 if(B.Num()>256){Invalidate(TEXT("At most 256 boundary components supported."));return false;}
 for(auto* X:B)
 {
  if(Resolve(GetWorld(),X->TargetSimulation,Error)!=this){Invalidate(Error);return false;}
  dem::Plane F;if(!X->Snapshot(F,Error)){Invalidate(Error);return false;}
  if(!NewFaces.IsEmpty()&&NewFaces.Last().Initial.id==F.id){Invalidate(TEXT("Boundary IDs must be unique (duplicate component registration is already deduplicated)."));return false;}
  FaceSource S;S.B=X;S.Initial=F;NewFaces.Add(S);
  bool Duplicate=false;for(const auto& Existing:Boundaries){dem::Plane Compare=F;Compare.id=Existing.id;Duplicate|=SameFace(Existing,Compare);}
  if(!Duplicate)Boundaries.push_back(F);
 }
 dem::Config C;C.r=Common.RadiusMeters;C.rho=Common.DensityKgPerCubicMeter;C.kn=Common.NormalStiffness;C.kt=Common.TangentialStiffness;
 C.zn=Common.NormalDampingRatio;C.zt=Common.TangentialDampingRatio;C.mu=Common.ParticleFriction;C.muWall=Common.WallFriction;C.dt=FixedDt;C.gravity=DEMCoordinates::PolarToDEM(Gravity);
 if(!Solver.resetExternal(C,Initial,Boundaries)){Invalidate(UTF8_TO_TCHAR(Solver.error));return false;}
 if(FixedDt>.25*Solver.conservativeScale()){Invalidate(TEXT("Fixed dt exceeds 0.25*sqrt(min effective mass / max stiffness); reduce dt and Reset. This guard alone does not prove stability."));return false;}
 Sources=MoveTemp(NewSources);Faces=MoveTemp(NewFaces);
 for(auto& S:Sources)S.E->BindSimulation(this,S.First);
 bReady=true;bResetRequired=false;Lag=DisplayClock=0;FrameSubsteps=0;LastUploadFrame=MAX_uint64;
 EmitterCount=Sources.Num();BoundaryCount=int32(Boundaries.size());ParticleCount=int32(Initial.size());
 Status=TEXT("Ready / paused. World coordinates; manager transform ignored.");UpdateStats();Publish(true);return true;
}
bool ADEMWorld::CheckTopology()
{
 FString Error;if(Resolve(GetWorld(),this,Error)!=this){Invalidate(Error);return false;}
 TArray<ADEMEmitter*> E;TArray<UDEMPlaneBoundaryComponent*> B;Gather(GetWorld(),E,B);
 if(E.Num()!=Sources.Num()||B.Num()!=Faces.Num()){Invalidate(TEXT("Participant added/removed; Reset required."));return false;}
 for(int I=0;I<E.Num();++I) if(!Sources[I].E.IsValid()||Sources[I].E!=E[I]||Sources[I].Order!=E[I]->GenerationOrder||
 E[I]->GetBoundSimulation()!=this||Resolve(GetWorld(),E[I]->TargetSimulation,Error)!=this)
 {Invalidate(TEXT("Emitter identity/order/target changed; Reset required."));return false;}
 for(int I=0;I<B.Num();++I)
 {
  dem::Plane F;
  if(!Faces[I].B.IsValid()||Faces[I].B!=B[I]||Resolve(GetWorld(),B[I]->TargetSimulation,Error)!=this||!B[I]->Snapshot(F,Error)||!SameFace(F,Faces[I].Initial))
  {Invalidate(TEXT("Fixed boundary moved/changed/removed; Reset required. Moving walls are unsupported."));return false;}
 }
 const dem::V G=DEMCoordinates::PolarToDEM(Gravity);
 if(Solver.c.dt!=FixedDt || (Solver.c.gravity-G).norm2()!=0){Invalidate(TEXT("Physics dt/gravity changed; Reset required."));return false;}
 return true;
}
void ADEMWorld::Reset()
{
 bRunning=false;bReady=false;bResetRequired=false;bInitialAttempt=true;
 for(auto&S:Sources)if(S.E.IsValid()){S.E->ReleaseSimulation();S.E->Clear();}
 Sources.Empty();Faces.Empty();PendingEmitters.Empty();PendingBoundaries.Empty();Solver=dem::Solver{};
 ParticleCount=EmitterCount=BoundaryCount=0;Lag=SimulationTime=0;bNonFiniteOrFailed=false;
 Initialize();
}
void ADEMWorld::Start()
{
 if(bResetRequired)return;
 if(!bReady&&!Initialize())return;
 if(CheckTopology()&&CheckClock()){bRunning=true;Status=TEXT("Running fixed-step DEM.");}
}
void ADEMWorld::Pause(){bRunning=false;if(!bResetRequired)Status=TEXT("Paused; pending lag retained.");}
bool ADEMWorld::OneStep()
{
 if(!Solver.step()){bNonFiniteOrFailed=true;Invalidate(UTF8_TO_TCHAR(Solver.error));return false;}
 ++FrameSubsteps;UpdateStats();return true;
}
void ADEMWorld::Step()
{
 if(bResetRequired)return;
 if(!bReady&&!Initialize())return;
 Pause();FrameSubsteps=0;
 if(CheckClock()&&CheckTopology()&&OneStep()){if(Lag>=Solver.c.dt)Lag-=Solver.c.dt;Publish();}
}
void ADEMWorld::Advance(double Seconds)
{
 FrameSubsteps=0;
 if(!GetWorld()||!GetWorld()->HasBegunPlay())return;
 if(!bReady&&!bResetRequired){if(Initialize()){bInitialAttempt=true;bRunning=bAutoStart;}else return;}
 if(bResetRequired||!bReady||!CheckClock()||!CheckTopology())return;
 if(!std::isfinite(Seconds)||Seconds<0){Invalidate(TEXT("Nonfinite/negative frame delta."));return;}
 DisplayClock+=Seconds;
 if(bRunning)
 {
  const double Added=Seconds*SpeedMultiplier;
  if(!std::isfinite(Added)||!std::isfinite(Lag+Added)){Invalidate(TEXT("Accumulator overflow."));return;}
  Lag+=Added;
  if(Lag>=MaximumLag){bRunning=false;Status=TEXT("Lag limit reached: paused, all uncomputed time retained. Reduce speed or raise Max Substeps, and increase Maximum Lag to resume; Reset explicitly discards lag.");}
  else while(Lag>=Solver.c.dt && FrameSubsteps<MaxSubsteps){if(!OneStep())break;Lag-=Solver.c.dt;}
 }
 if(DisplayClock>=1/DisplayHz){Publish();DisplayClock=0;}
}
void ADEMWorld::Publish(bool Force)
{
 if(!bReady||(!Force&&LastUploadFrame==GFrameCounter))return;
 for(auto&S:Sources)if(S.E.IsValid())S.E->UploadSimulation(Solver,S.First,S.Count);
 LastUploadFrame=GFrameCounter;
}
void ADEMWorld::UpdateStats()
{
 SimulationTime=Solver.s.time;ContactCount=Solver.s.contacts;CandidatePairs=Solver.s.candidates;MaximumSpeed=Solver.s.maxSpeed;
 KineticEnergy=Solver.s.ke+Solver.s.rotKE;MaximumOverlapRatio=Solver.s.maxOverlap;StepMilliseconds=Solver.s.stepMs;
}
bool ADEMWorld::SetExternalLoad(int32 ID,FVector F,FVector T)
{
 if(!bReady||bResetRequired)return false;
 if(ID<0||!Solver.setExternalLoad(uint32(ID),DEMCoordinates::PolarToDEM(F),DEMCoordinates::AxialToDEM(T)))
 {Invalidate(TEXT("Invalid external force/torque or global ID."));return false;}return true;
}
void ADEMWorld::ClearExternalLoads(){Solver.clearExternalLoads();}
void ADEMWorld::BeginPlay(){Super::BeginPlay();Status=TEXT("Waiting until first frame after BeginPlay registration.");}
void ADEMWorld::Tick(float D){Super::Tick(D);Advance(double(D));}
void ADEMWorld::EndPlay(const EEndPlayReason::Type R)
{
 bRunning=false;for(auto&S:Sources)if(S.E.IsValid()){S.E->ReleaseSimulation();S.E->Clear();}
 Sources.Empty();Faces.Empty();bReady=false;Super::EndPlay(R);
}
