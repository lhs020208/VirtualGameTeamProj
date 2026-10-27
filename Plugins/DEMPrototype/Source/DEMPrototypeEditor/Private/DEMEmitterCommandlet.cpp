#include "DEMEmitterCommandlet.h"
#include "DEMEmitter.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "StaticMeshCompiler.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace
{
constexpr const TCHAR* BPPath = TEXT("/Game/DEM/Emitters/BP_DEMEmitter");
constexpr const TCHAR* SettingsPath = TEXT("/Game/DEM/Settings/DA_DEMParticle_Default.DA_DEMParticle_Default");
UWorld* MakeWorld(EWorldType::Type Type)
{
    UWorld::InitializationValues Values;
    Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
        .CreateAISystem(false).ShouldSimulatePhysics(false).CreateFXSystem(false).SetTransactional(false);
    return UWorld::CreateWorld(Type, false, NAME_None, nullptr, true, ERHIFeatureLevel::SM5, &Values);
}
bool Same(const TArray<FDEMEmittedParticle>& A, const TArray<FDEMEmittedParticle>& B)
{
    if (A.Num() != B.Num()) return false;
    for (int32 I = 0; I < A.Num(); ++I)
        if (A[I].LocalIndex != B[I].LocalIndex || A[I].PositionMeters != B[I].PositionMeters ||
            A[I].LinearVelocityMetersPerSecond != B[I].LinearVelocityMetersPerSecond ||
            A[I].AngularVelocityRadiansPerSecond != B[I].AngularVelocityRadiansPerSecond ||
            A[I].Orientation != B[I].Orientation) return false;
    return true;
}
}

UDEMEmitterCommandlet::UDEMEmitterCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 UDEMEmitterCommandlet::Main(const FString& Params)
{
    const bool Create = FParse::Param(*Params, TEXT("CreateBlueprint"));
    const bool Verify = FParse::Param(*Params, TEXT("Verify"));
    if (Create == Verify) return 1;
    UDEMParticleSettings* Settings = LoadObject<UDEMParticleSettings>(nullptr, SettingsPath);
    if (!Settings)
    {
        UE_LOG(LogTemp, Error, TEXT("Original project cannot load UDEMParticleSettings/default asset. Build original project first."));
        return 2;
    }
    FStaticMeshCompilingManager::Get().FinishAllCompilation();
    FDEMParticleRenderSnapshot SettingsSnapshot;
    TArray<FText> Errors;
    if (!Settings->TryCreateRenderSnapshot(SettingsSnapshot, Errors)) return 3;
    UBlueprint* BP = nullptr;
    const bool Exists = FPackageName::DoesPackageExist(BPPath);
    if (Exists)
    {
        BP = LoadObject<UBlueprint>(nullptr, *(FString(BPPath) + TEXT(".BP_DEMEmitter")));
        UE_LOG(LogTemp, Display, TEXT("Existing BP inspected, no overwrite: %s"), BPPath);
    }
    else if (Create)
    {
        UPackage* Package = CreatePackage(BPPath);
        BP = FKismetEditorUtilities::CreateBlueprint(ADEMEmitter::StaticClass(), Package,
            TEXT("BP_DEMEmitter"), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("DEMEmitterCreation"));
        if (!BP) return 4;
        FKismetEditorUtilities::CompileBlueprint(BP);
        if (BP->Status == BS_Error || !BP->GeneratedClass) return 5;
        ADEMEmitter* Defaults = Cast<ADEMEmitter>(BP->GeneratedClass->GetDefaultObject());
        if (!Defaults) return 6;
        Defaults->ParticleSettings = Settings;
        BP->MarkPackageDirty();
        const FString Filename = FPackageName::LongPackageNameToFilename(BPPath, FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        if (!UPackage::SavePackage(Package, BP, *Filename, Args)) return 7;
        UE_LOG(LogTemp, Display, TEXT("Saved BP_DEMEmitter in original project: %s"), *Filename);
    }
    if (!BP || !BP->GeneratedClass || !BP->GeneratedClass->IsChildOf(ADEMEmitter::StaticClass())) return 8;
    auto* Defaults = Cast<ADEMEmitter>(BP->GeneratedClass->GetDefaultObject());
    if (!Defaults || Defaults->ParticleSettings != Settings)
    {
        UE_LOG(LogTemp, Error, TEXT("Existing BP has different settings; preserved, not overwritten."));
        return 9;
    }
    if (Create) return 0;

    FString Report = TEXT("Original-project fresh-process DEM emitter tests; no saved test map, no DEM motion.\n");
    int Checks = 0, Failures = 0;
    auto Check = [&](bool Good, const FString& Name)
    {
        ++Checks;
        if (!Good) ++Failures;
        const FString Line = FString::Printf(TEXT("%s %s\n"), Good ? TEXT("PASS") : TEXT("FAIL"), *Name);
        Report += Line;
        UE_LOG(LogTemp, Display, TEXT("%s"), *Line);
    };
    Check(BP->ParentClass == ADEMEmitter::StaticClass() && Defaults->ParticleSettings == Settings,
        TEXT("fresh reload BP parent/native settings class/default reference"));
    Check(!Defaults->PrimaryActorTick.bCanEverTick, TEXT("emitter has no per-frame tick"));
    for (const FName Name : {FName(TEXT("Generate")), FName(TEXT("Clear")), FName(TEXT("RefreshPreview"))})
    {
        const UFunction* Function = ADEMEmitter::StaticClass()->FindFunctionByName(Name);
        Check(Function && Function->GetBoolMetaData(TEXT("CallInEditor")) && Function->ParmsSize == 0,
            Name.ToString() + TEXT(" is a valid zero-parameter Details button"));
    }
    const bool SettingsDirtyBefore = Settings->GetPackage()->IsDirty();
    UWorld* World = MakeWorld(EWorldType::Game);
    if (!World) return 10;
    ADEMEmitter* A = World->SpawnActor<ADEMEmitter>(BP->GeneratedClass.Get());
    if (!A) return 11;
    Check(A->GeneratedCount == 0, TEXT("game construction does not duplicate preview"));
    World->InitializeActorsForPlay(FURL());
    // Real engine BeginPlay dispatch without GameMode/HUD/camera/level blueprint.
    World->GetWorldSettings()->NotifyBeginPlay();
    FDEMEmitterBurst Burst;
    Check(A->HasActorBegunPlay() && A->CopyGeneratedBurst(Burst) && Burst.Particles.Num() == 200 &&
        A->GetParticleInstances()->GetInstanceCount() == 200, TEXT("engine BeginPlay automatically generates exact BP default count"));
    Check(World->GetAuthGameMode() == nullptr, TEXT("no GameMode required"));
    int32 EmitterActors = 0;
    bool NoFrameworkActors = true;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        EmitterActors += It->IsA<ADEMEmitter>() ? 1 : 0;
        const FString Name = It->GetClass()->GetName();
        NoFrameworkActors &= !Name.Contains(TEXT("Camera")) && !Name.Contains(TEXT("HUD")) && !Name.Contains(TEXT("DEMSimulation"));
    }
    Check(EmitterActors == 1 && NoFrameworkActors, TEXT("one emitter actor, no camera/HUD/DEMSimulationActor or per-particle actors"));
    const auto* ISM = A->GetParticleInstances();
    Check(ISM->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !ISM->IsSimulatingPhysics() &&
        !ISM->IsPhysicsStateCreated() && !ISM->CanEverAffectNavigation() && !ISM->CastShadow && !ISM->bCastDynamicShadow &&
        !ISM->PrimaryComponentTick.bCanEverTick, TEXT("ISM: no collision/Chaos state/navigation/shadows/tick"));

    A->ParticleCount = 23;
    A->ColumnsX = 4;
    A->ColumnsY = 3;
    A->SurfaceGap = .002;
    Check(A->TryGenerate() && A->CopyGeneratedBurst(Burst) && Burst.Particles.Num() == 23, TEXT("manual burst exact non-full-layer count"));
    bool Layout = Burst.Particles.Num() == 23, NoOverlap = true;
    const double Pitch = .022;
    for (int I = 0; I < Burst.Particles.Num(); ++I)
    {
        const FVector3d Expected((I % 4 - 1.5) * Pitch, -((I / 4) % 3 - 1.0) * Pitch, .01 + (I / 12) * Pitch);
        Layout &= Burst.Particles[I].LocalIndex == I && Burst.Particles[I].PositionMeters.Equals(Expected, 1.e-12);
        for (int J = 0; J < I; ++J)
            NoOverlap &= (Burst.Particles[I].PositionMeters - Burst.Particles[J].PositionMeters).Length() >= .02 - 1.e-12;
    }
    Check(Layout, TEXT("centered XY; first layer Z=radius; X-fastest then Y then Z; stable local indices"));
    Check(NoOverlap, TEXT("all-pairs nonoverlap within emitter"));
    FDEMEmitterBurst Repeated;
    Check(A->TryGenerate() && A->CopyGeneratedBurst(Repeated) && Same(Burst.Particles, Repeated.Particles) &&
        ISM->GetInstanceCount() == 23, TEXT("repeat Generate replaces; deterministic order and positions, no accumulation"));
    A->Clear();
    Check(A->GeneratedCount == 0 && ISM->GetInstanceCount() == 0 && !A->CopyGeneratedBurst(Repeated) && Repeated.Particles.IsEmpty(),
        TEXT("Clear removes display/data and invalidates consumer copy"));

    UDEMParticleSettings* Copy = DuplicateObject<UDEMParticleSettings>(Settings, GetTransientPackage());
    A->ParticleSettings = Copy;
    Copy->RadiusMeters = .02;
    A->TryGenerate();
    A->CopyGeneratedBurst(Repeated);
    FTransform Instance0, Instance1;
    ISM->GetInstanceTransform(0, Instance0, true);
    ISM->GetInstanceTransform(1, Instance1, true);
    Check(Repeated.Particles.Num() == 23 && FMath::IsNearlyEqual((Instance1.GetLocation() - Instance0.GetLocation()).Length(), 4.2, 1.e-5) &&
        FMath::IsNearlyEqual(Instance0.GetScale3D().X * SettingsSnapshot.SourceRadiusCm, 2.0, 1.e-6) &&
        Repeated.Settings.RadiusMeters == .02, TEXT("radius edit + regenerate updates snapshot, pitch and physical display radius"));
    Check(Settings->RadiusMeters == .01 && Burst.Settings.RadiusMeters == .01,
        TEXT("shared default asset and earlier copied burst unchanged by transient settings edit"));

    A->ParticleSettings = Settings;
    A->InitialLinearVelocity = FVector(1, 2, 3);
    A->InitialAngularVelocity = FVector(4, -5, 6);
    const FQuat Rotation = FRotator(0, 90, 0).Quaternion();
    A->SetActorTransform(FTransform(Rotation, FVector(100, 200, 300)));
    Check(A->TryGenerate() && A->CopyGeneratedBurst(Repeated), TEXT("translated/rotated generation"));
    if (!Repeated.Particles.IsEmpty())
    {
        const auto& P = Repeated.Particles[0];
        const FVector3d ExpectedUE = FVector(1, 2, 3) + Rotation.RotateVector(FVector(-.033, -.022, .01));
        Check(P.PositionMeters.Equals(FVector(ExpectedUE.X, -ExpectedUE.Y, ExpectedUE.Z), 1.e-12), TEXT("translation cm->m and rotated local grid -> DEM polar position"));
        Check(P.LinearVelocityMetersPerSecond.Equals(FVector(-2, -1, 3), 1.e-12), TEXT("local linear velocity rotates then polar Y reflection"));
        Check(P.AngularVelocityRadiansPerSecond.Equals(FVector(-5, 4, -6), 1.e-12), TEXT("local angular velocity rotates then axial (-X,+Y,-Z), not polar reflection"));
        const FVector3d RHLocal(1, -2, 3), UERotated = Rotation.RotateVector(FVector(1, 2, 3));
        Check(P.Orientation.RotateVector(RHLocal).Equals(FVector(UERotated.X, -UERotated.Y, UERotated.Z), 1.e-12), TEXT("quaternion handedness matches vector reflection"));
    }
    FTransform FixedBefore, FixedAfter;
    ISM->GetInstanceTransform(0, FixedBefore, true);
    A->SetActorLocationAndRotation(FVector(900, -800, 700), FRotator(30, 50, 70));
    ISM->GetInstanceTransform(0, FixedAfter, true);
    FDEMEmitterBurst FixedData;
    Check(A->CopyGeneratedBurst(FixedData) && Same(FixedData.Particles, Repeated.Particles) && FixedBefore.Equals(FixedAfter, 1.e-9),
        TEXT("moving/rotating emitter after generation leaves world display and SI data fixed"));
    A->SetActorScale3D(FVector(2, 1, 1));
    Check(!A->TryGenerate() && A->GenerationStatus.Contains(TEXT("Scale")) && ISM->GetInstanceCount() == 0 && !A->CopyGeneratedBurst(FixedData),
        TEXT("non-unit scale rejects regeneration and clears old burst"));
    A->SetActorTransform(FTransform::Identity);

    ADEMEmitter* B = World->SpawnActor<ADEMEmitter>(BP->GeneratedClass.Get());
    B->ParticleCount = 7;
    B->TryGenerate();
    FDEMEmitterBurst BBefore, BAfter;
    B->CopyGeneratedBurst(BBefore);
    A->TryGenerate(); A->Clear();
    B->CopyGeneratedBurst(BAfter);
    Check(Same(BBefore.Particles, BAfter.Particles) && B->VisibleInstanceCount == 7, TEXT("two emitters independent Generate/Clear state"));

    auto Invalid = [&](const TCHAR* Name)
    {
        FDEMEmitterBurst InvalidCopy;
        const bool Rejected = !A->TryGenerate();
        Check(Rejected && A->GenerationStatus.StartsWith(TEXT("ERROR:")) && A->VisibleInstanceCount == 0 &&
            A->GeneratedCount == 0 && !A->CopyGeneratedBurst(InvalidCopy) && InvalidCopy.Particles.IsEmpty(), Name);
        Report += TEXT("  ") + A->GenerationStatus + TEXT("\n");
    };
    A->ParticleSettings = nullptr; Invalid(TEXT("null settings"));
    A->ParticleSettings = Copy; Copy->SphereMesh = nullptr; Invalid(TEXT("missing mesh"));
    Copy->SphereMesh = Settings->SphereMesh; Copy->RadiusMeters = -1; Invalid(TEXT("invalid physical snapshot"));
    A->ParticleSettings = Settings;
    A->ParticleCount = -1; Invalid(TEXT("negative count"));
    A->ParticleCount = ADEMEmitter::MaxParticleCount + 1; Invalid(TEXT("over count cap"));
    A->ParticleCount = 23;
    A->ColumnsX = 0; Invalid(TEXT("zero columns"));
    A->ColumnsX = std::numeric_limits<int32>::max(); Invalid(TEXT("overflow-sized columns"));
    A->ColumnsX = 4;
    A->ColumnsY = -1; Invalid(TEXT("negative columns Y")); A->ColumnsY = 3;
    A->SurfaceGap = -1; Invalid(TEXT("negative gap"));
    A->SurfaceGap = std::numeric_limits<double>::quiet_NaN(); Invalid(TEXT("NaN gap"));
    A->SurfaceGap = std::numeric_limits<double>::infinity(); Invalid(TEXT("Inf gap"));
    A->SurfaceGap = std::numeric_limits<double>::max(); Invalid(TEXT("grid arithmetic overflow/range"));
    A->SurfaceGap = .002;
    A->InitialLinearVelocity.X = std::numeric_limits<double>::quiet_NaN(); Invalid(TEXT("NaN linear velocity"));
    A->InitialLinearVelocity = FVector::ZeroVector;
    A->InitialAngularVelocity.Z = std::numeric_limits<double>::infinity(); Invalid(TEXT("Inf angular velocity"));
    A->InitialAngularVelocity = FVector::ZeroVector;
    A->SetActorScale3D(FVector(-1)); Invalid(TEXT("negative scale"));
    A->SetActorTransform(FTransform::Identity);
    A->ParticleCount = ADEMEmitter::MaxParticleCount;
    A->ColumnsX = A->ColumnsY = 100;
    Check(A->TryGenerate() && A->GeneratedCount == ADEMEmitter::MaxParticleCount && A->VisibleInstanceCount == ADEMEmitter::MaxParticleCount,
        TEXT("maximum allowed burst generates full 100000, no silent truncation"));
    A->ParticleCount = 0;
    Check(A->TryGenerate() && A->CopyGeneratedBurst(Repeated) && Repeated.Particles.IsEmpty() && A->VisibleInstanceCount == 0,
        TEXT("zero-count burst is explicitly empty and valid"));

    UWorld* PreviewWorld = MakeWorld(EWorldType::EditorPreview);
    if (!PreviewWorld) return 12;
    ADEMEmitter* Preview = PreviewWorld->SpawnActor<ADEMEmitter>(BP->GeneratedClass.Get());
    if (!Preview) return 13;
    Preview->ParticleCount = ADEMEmitter::MaxPreviewCount + 9;
    Preview->RefreshPreview();
    Check(Preview->bPreviewOnly && Preview->VisibleInstanceCount == ADEMEmitter::MaxPreviewCount && Preview->GeneratedCount == 0 &&
        Preview->ParticleCount == ADEMEmitter::MaxPreviewCount + 9 && !Preview->CopyGeneratedBurst(Repeated),
        TEXT("preview cap visible separately; request unchanged; no solver handoff"));
    Preview->SetActorScale3D(FVector(2)); Preview->RefreshPreview();
    Check(Preview->GenerationStatus.Contains(TEXT("Scale")) && Preview->VisibleInstanceCount == 0, TEXT("editor preview rejects non-unit scale"));
    Preview->SetActorScale3D(FVector::OneVector); Preview->RefreshPreview();
    // Copy a preview-configured actor as a level/PIE-style template into a game world.
    FActorSpawnParameters Spawn;
    Spawn.Template = Preview;
    ADEMEmitter* FromPreview = World->SpawnActor<ADEMEmitter>(BP->GeneratedClass.Get(), FTransform::Identity, Spawn);
    Check(FromPreview && FromPreview->HasActorBegunPlay() && !FromPreview->bPreviewOnly &&
        FromPreview->GeneratedCount == ADEMEmitter::MaxPreviewCount + 9 && FromPreview->VisibleInstanceCount == ADEMEmitter::MaxPreviewCount + 9,
        TEXT("preview template -> game BeginPlay full count, no doubled instances"));
    ADEMEmitter* Disabled = PreviewWorld->SpawnActor<ADEMEmitter>(BP->GeneratedClass.Get());
    Disabled->bGenerateOnBeginPlay = false;
    Disabled->RefreshPreview();
    Spawn.Template = Disabled;
    ADEMEmitter* DisabledGame = World->SpawnActor<ADEMEmitter>(BP->GeneratedClass.Get(), FTransform::Identity, Spawn);
    Check(DisabledGame && DisabledGame->HasActorBegunPlay() && DisabledGame->VisibleInstanceCount == 0 &&
        !DisabledGame->CopyGeneratedBurst(Repeated), TEXT("BeginPlay disabled clears inherited preview"));
    A->ParticleCount = 3; A->TryGenerate();
    const bool DestroyResult = A->Destroy();
    Check(DestroyResult && !A->CopyGeneratedBurst(Repeated) && A->GetParticleInstances()->GetInstanceCount() == 0 && B->VisibleInstanceCount == 7,
        TEXT("destroy emitter clears own data/display, other emitter unaffected"));
    Check(Settings->GetPackage()->IsDirty() == SettingsDirtyBefore && Settings->RadiusMeters == .01 && Settings->NormalStiffness == 500000 &&
        Settings->SphereMesh && Settings->SphereMesh->GetPathName() == TEXT("/Game/DEM/SM_DEMSphere.SM_DEMSphere"),
        TEXT("shared settings values/reference/dirty flag unchanged"));

    World->EndPlay(EEndPlayReason::Quit);
    Check(!B->CopyGeneratedBurst(Repeated) && B->VisibleInstanceCount == 0,
        TEXT("world EndPlay clears remaining emitter data/display"));
    PreviewWorld->DestroyWorld(false);
    World->DestroyWorld(false);
    Report += FString::Printf(TEXT("TOTAL checks=%d failures=%d\n"), Checks, Failures);
    const FString Output = FPaths::ProjectPluginsDir() / TEXT("DEMPrototype/EmitterValidation/verification.txt");
    if (!FFileHelper::SaveStringToFile(Report, *Output)) return 14;
    return Failures ? 15 : 0;
}
