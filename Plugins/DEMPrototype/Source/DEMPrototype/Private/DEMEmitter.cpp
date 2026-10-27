#include "DEMEmitter.h"
#include "DEMWorld.h"
#include "DEMCoordinates.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/ArrowComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include <cmath>
#include <limits>

DEFINE_LOG_CATEGORY_STATIC(LogDEMEmitter, Log, All);

namespace
{
bool Finite(const FVector3d& V)
{
    return std::isfinite(V.X) && std::isfinite(V.Y) && std::isfinite(V.Z);
}
}

ADEMEmitter::ADEMEmitter()
{
    PrimaryActorTick.bCanEverTick = false;
    EmitterRoot = CreateDefaultSubobject<USceneComponent>(TEXT("EmitterOrigin"));
    SetRootComponent(EmitterRoot);
    Instances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Particles"));
    Instances->SetFlags(RF_Transient);
    Instances->SetupAttachment(EmitterRoot);
    // Absolute component transform freezes the burst anchor when the emitter moves.
    Instances->SetAbsolute(true, true, true);
    Instances->SetMobility(EComponentMobility::Movable);
    Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Instances->SetGenerateOverlapEvents(false);
    Instances->SetSimulatePhysics(false);
    Instances->bAlwaysCreatePhysicsState = false;
    Instances->SetCanEverAffectNavigation(false);
    Instances->SetCastShadow(false);
    Instances->bCastDynamicShadow = false;
    Instances->PrimaryComponentTick.bCanEverTick = false;
#if WITH_EDITORONLY_DATA
    Direction = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("LocalZDirection"));
    if (Direction)
    {
        Direction->SetupAttachment(EmitterRoot);
        Direction->SetRelativeRotation(FRotator(90, 0, 0));
        Direction->ArrowColor = FColor::Cyan;
        Direction->ArrowSize = 0.5f;
        Direction->SetHiddenInGame(true);
        Direction->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Direction->SetCanEverAffectNavigation(false);
    }
#endif
    GenerationStatus = TEXT("Not generated. Static burst only; no DEM motion.");
}

void ADEMEmitter::Clear()
{
    if (ADEMWorld* W=BoundSimulation.Get()) {W->Invalidate(TEXT("Connected emitter Clear requested; use common Reset."));return;}
    bValidBurst = false;
    Particles.Empty();
    ActiveSettings = {};
    GeneratedCount = VisibleInstanceCount = 0;
    bPreviewOnly = false;
    if (Instances)
    {
        Instances->ClearInstances();
        Instances->EmptyOverrideMaterials();
        Instances->SetStaticMesh(nullptr);
    }
    GenerationStatus = TEXT("Cleared: no data or instances.");
}

bool ADEMEmitter::Fail(const FString& Reason)
{
    Clear();
    GenerationStatus = TEXT("ERROR: ") + Reason;
    UE_LOG(LogDEMEmitter, Warning, TEXT("%s: %s"), *GetName(), *GenerationStatus);
    return false;
}

bool ADEMEmitter::BuildBurst(bool bPreview)
{
    check(IsInGameThread());
    Clear(); // Transactional replacement: even failed regeneration invalidates old data.
    if (!ParticleSettings) return Fail(TEXT("Particle Settings is null. Select a DEM Particle Settings asset."));
    FDEMParticleRenderSnapshot Candidate;
    TArray<FText> Errors;
    if (!ParticleSettings->TryCreateRenderSnapshot(Candidate, Errors))
    {
        FString Message;
        for (const FText& E : Errors) Message += E.ToString() + TEXT("; ");
        return Fail(Message);
    }
    if (ParticleCount < 0 || ParticleCount > MaxParticleCount)
        return Fail(TEXT("Particle Count must be 0..100000. Use Clear for an empty burst; no automatic truncation."));
    if (ColumnsX < 1 || ColumnsY < 1 || ColumnsX > MaxParticleCount || ColumnsY > MaxParticleCount)
        return Fail(TEXT("Columns X / Y must each be 1..100000."));
    if (!std::isfinite(SurfaceGap) || SurfaceGap < 0)
        return Fail(TEXT("Surface Gap must be finite and >= 0 meters."));
    if (!Finite(InitialLinearVelocity) || !Finite(InitialAngularVelocity))
        return Fail(TEXT("Initial Linear / Angular Velocity must contain only finite values."));
    const FTransform Anchor = GetActorTransform();
    const FVector Scale = Anchor.GetScale3D();
    const FQuat Q = Anchor.GetRotation();
    if (!Finite(Scale) || !Scale.Equals(FVector::OneVector, 1.e-8))
        return Fail(TEXT("Actor world Scale must be (1,1,1), including parent scale. Change Radius in Particle Settings instead."));
    if (!Finite(Anchor.GetLocation()) || Anchor.GetLocation().GetAbsMax() > 1.e8 || Q.ContainsNaN() || !Q.IsNormalized())
        return Fail(TEXT("Emitter transform must be finite, rotation normalized, and origin within +/-1000000 meters."));
    const double Radius = Candidate.Physics.RadiusMeters;
    const double Diameter = Candidate.Physics.Derived.DiameterMeters;
    const double Pitch = Diameter + SurfaceGap;
    if (!std::isnormal(Pitch) || Pitch < Diameter || (SurfaceGap > 0 && Pitch <= Diameter))
        return Fail(TEXT("Surface Gap + diameter overflows/underflows or gap cannot be represented."));
    const int64 PerLayer = int64(ColumnsX) * int64(ColumnsY);
    const int64 LastLayer = ParticleCount ? (int64(ParticleCount) - 1) / PerLayer : 0;
    const FVector3d Extent((ColumnsX - 1) * 0.5 * Pitch, (ColumnsY - 1) * 0.5 * Pitch, Radius + LastLayer * Pitch);
    if (!Finite(Extent) || Extent.GetAbsMax() > 10000)
        return Fail(TEXT("Grid extent overflows or exceeds the 10000 meter local display limit. Reduce columns/count/gap/radius."));
    // Reject layouts where floating-point coordinate resolution can erase particle separation.
    const double RenderResolution = 8 * std::numeric_limits<float>::epsilon() * Extent.GetAbsMax();
    const double WorldResolution = 64 * std::numeric_limits<double>::epsilon() * (Anchor.GetLocation().GetAbsMax() * .01 + Extent.GetAbsMax());
    if (Diameter < FMath::Max(RenderResolution, WorldResolution))
        return Fail(TEXT("Radius is too small relative to grid/world coordinates for reliable non-overlapping display precision."));
    const FVector3d Velocity = DEMCoordinates::Polar(Q.RotateVector(InitialLinearVelocity));
    const FVector3d Spin = DEMCoordinates::Axial(Q.RotateVector(InitialAngularVelocity));
    if (!Finite(Velocity) || !Finite(Spin)) return Fail(TEXT("Initial velocity rotation overflowed."));
    const FQuat4d Orientation=DEMCoordinates::Orientation(Q);
    const int32 Count = bPreview ? FMath::Min(ParticleCount, MaxPreviewCount) : ParticleCount;
    TArray<FDEMEmittedParticle> NewParticles;
    TArray<FTransform> Transforms;
    if (!bPreview) NewParticles.Reserve(Count);
    Transforms.Reserve(Count);
    for (int32 I = 0; I < Count; ++I)
    {
        const int64 Layer = int64(I) / PerLayer;
        const int32 X = I % ColumnsX;
        const int32 Y = int32((int64(I) / ColumnsX) % ColumnsY);
        const FVector3d Local((X - (ColumnsX - 1) * 0.5) * Pitch,
            (Y - (ColumnsY - 1) * 0.5) * Pitch, Radius + Layer * Pitch);
        const FVector3d WorldUE = Anchor.GetLocation() * .01 + Q.RotateVector(Local);
        if (!Finite(WorldUE)) return Fail(TEXT("World position arithmetic overflowed."));
        if (!bPreview)
        {
            FDEMEmittedParticle& P = NewParticles.AddDefaulted_GetRef();
            P.LocalIndex = I;
            P.PositionMeters = DEMCoordinates::Polar(WorldUE);
            P.LinearVelocityMetersPerSecond = Velocity;
            P.AngularVelocityRadiansPerSecond = Spin;
            P.Orientation = Orientation;
        }
        Transforms.Emplace(FQuat::Identity, Local * 100, FVector(Candidate.UniformScale));
    }
    Instances->SetAbsolute(true, true, true);
    Instances->SetWorldTransform(FTransform(Q, Anchor.GetLocation(), FVector::OneVector));
    Instances->SetStaticMesh(Candidate.SphereMesh);
    if (Candidate.MaterialOverride) Instances->SetMaterial(0, Candidate.MaterialOverride);
    Instances->AddInstances(Transforms, false, false, false); // One batch, no per-particle dirty calls.
    if (Instances->GetInstanceCount() != Count) return Fail(TEXT("ISM did not create the requested instance count."));
    ActiveSettings = Candidate;
    Particles = MoveTemp(NewParticles);
    bValidBurst = !bPreview;
    bPreviewOnly = bPreview;
    GeneratedCount = Particles.Num();
    VisibleInstanceCount = Count;
    GenerationStatus = bPreview
        ? FString::Printf(TEXT("PREVIEW %d / requested %d (cap %d). No solver data. Refresh Preview after asset edits."), Count, ParticleCount, MaxPreviewCount)
        : FString::Printf(TEXT("GENERATED %d / requested %d. Static display; velocities stored only, no gravity/contact motion."), Count, ParticleCount);
    return true;
}

void ADEMEmitter::Generate() { TryGenerate(); }
bool ADEMEmitter::TryGenerate()
{
 if(ADEMWorld* W=BoundSimulation.Get()) { W->Invalidate(TEXT("Connected emitter Generate requested; use common Reset."));return false; }
 return BuildBurst(false);
}

void ADEMEmitter::RefreshPreview()
{
    if (GetWorld() && GetWorld()->IsGameWorld()) return; // Never overwrite a playing burst.
    if (bEditorPreview) BuildBurst(true);
    else Clear();
}

void ADEMEmitter::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (GetWorld() && !GetWorld()->IsGameWorld()) RefreshPreview();
}

void ADEMEmitter::BeginPlay()
{
    Super::BeginPlay();
    Clear();
    if (bGenerateOnBeginPlay) Generate();
    FString Error;
    if(ADEMWorld* W=ADEMWorld::Resolve(GetWorld(),TargetSimulation,Error)) W->RegisterEmitter(this);
    else { GenerationStatus += TEXT(" Standalone: ")+Error; UE_LOG(LogDEMEmitter,Warning,TEXT("%s: %s"),*GetName(),*Error); }
}

bool ADEMEmitter::CopyGeneratedBurst(FDEMEmitterBurst& OutBurst) const
{
    check(IsInGameThread());
    OutBurst = {};
    if (!bValidBurst || BoundSimulation.IsValid()) return false;
    OutBurst.Settings = ActiveSettings.Physics;
    OutBurst.Particles = Particles;
    return true;
}

void ADEMEmitter::EndPlay(const EEndPlayReason::Type Reason)
{
    if(ADEMWorld* W=BoundSimulation.Get()) W->Invalidate(TEXT("Registered emitter removed; Reset required."));
    ReleaseSimulation(); Clear();
    Super::EndPlay(Reason);
}

void ADEMEmitter::Destroyed()
{
    if(ADEMWorld* W=BoundSimulation.Get()) W->Invalidate(TEXT("Registered emitter destroyed; Reset required."));
    ReleaseSimulation(); Clear();
    Super::Destroyed();
}

ADEMWorld* ADEMEmitter::GetBoundSimulation() const {return BoundSimulation.Get();}
void ADEMEmitter::BindSimulation(ADEMWorld* W,int32 First)
{
 BoundSimulation=W;FirstGlobalID=First;Particles.Empty();bValidBurst=false;
 SimulationTransforms.Reserve(GeneratedCount);
 GenerationStatus=TEXT("Owned by common DEM world; Generate/Clear require common Reset.");
}
void ADEMEmitter::ReleaseSimulation(){BoundSimulation.Reset();FirstGlobalID=-1;}
void ADEMEmitter::UploadSimulation(const dem::Solver& S,int32 First,int32 Count)
{
 if(!BoundSimulation.IsValid() || First<0 || size_t(First+Count)>S.p.size() || Instances->GetInstanceCount()!=Count) return;
 SimulationTransforms.Reset();
 for(int32 I=0;I<Count;++I) {const auto& P=S.p[First+I];SimulationTransforms.Emplace(DEMCoordinates::Quaternion(P.q),DEMCoordinates::PositionToUE(P.p),FVector(ActiveSettings.UniformScale));}
 Instances->BatchUpdateInstancesTransforms(0,SimulationTransforms,true,true,true);
}
