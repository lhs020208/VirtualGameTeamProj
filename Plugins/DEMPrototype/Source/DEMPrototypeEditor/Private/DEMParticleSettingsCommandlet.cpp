#include "DEMParticleSettingsCommandlet.h"
#include "DEMParticleSettings.h"
#include "Engine/StaticMesh.h"
#include "Factories/DataAssetFactory.h"
#include "HAL/FileManager.h"
#include "MeshDescription.h"
#include "Misc/DataValidation.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshResources.h"
#include "UObject/Package.h"
#include "UObject/LinkerLoad.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include <cmath>
#include <limits>

namespace
{
constexpr const TCHAR* PackageName = TEXT("/Game/DEM/Settings/DA_DEMParticle_Default");
constexpr const TCHAR* AssetName = TEXT("DA_DEMParticle_Default");
constexpr const TCHAR* MeshPath = TEXT("/Game/DEM/SM_DEMSphere.SM_DEMSphere");

bool Near(double A, double B, double Relative = 1.e-12)
{
    return std::isfinite(A) && std::isfinite(B) && std::abs(A - B) <= Relative * std::abs(B);
}

FString Describe(const UDEMParticleSettings* Asset)
{
    FString Result;
    for (TFieldIterator<FProperty> It(Asset->GetClass()); It; ++It)
    {
        FString Value;
        It->ExportText_InContainer(0, Value, Asset, Asset, const_cast<UDEMParticleSettings*>(Asset), PPF_None);
        Result += It->GetName() + TEXT("=") + Value + TEXT("\n");
    }
    return Result;
}

// Transient test meshes only. Every generated vertex is on the specified sphere
// before the requested offset/stretch; no existing mesh is edited or saved.
UStaticMesh* MakeSphere(double Radius, FVector Center = FVector::ZeroVector, FVector Stretch = FVector::OneVector)
{
    UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage());
    FMeshDescription MD;
    FStaticMeshAttributes Attr(MD);
    Attr.Register();
    auto Positions = Attr.GetVertexPositions();
    auto Normals = Attr.GetVertexInstanceNormals();
    auto UVs = Attr.GetVertexInstanceUVs();
    UVs.SetNumChannels(1);
    const auto Group = MD.CreatePolygonGroup();
    TArray<FVertexID> Vertices;
    constexpr int Longitude = 16, Latitude = 8;
    for (int J = 0; J <= Latitude; ++J)
        for (int I = 0; I < Longitude; ++I)
        {
            const double Theta = UE_DOUBLE_PI * J / Latitude, Phi = 2 * UE_DOUBLE_PI * I / Longitude;
            const FVector P = Center + Stretch * FVector(std::sin(Theta) * std::cos(Phi), std::sin(Theta) * std::sin(Phi), std::cos(Theta)) * Radius;
            const auto V = MD.CreateVertex();
            Positions[V] = FVector3f(P);
            Vertices.Add(V);
        }
    auto Triangle = [&](int A, int B, int C)
    {
        TArray<FVertexInstanceID> Instances;
        for (int Index : {A, B, C})
        {
            const auto V = MD.CreateVertexInstance(Vertices[Index]);
            Normals[V] = Positions[Vertices[Index]].GetSafeNormal();
            UVs.Set(V, 0, FVector2f(float(Index % Longitude) / Longitude, float(Index / Longitude) / Latitude));
            Instances.Add(V);
        }
        MD.CreatePolygon(Group, Instances);
    };
    for (int J = 0; J < Latitude; ++J)
        for (int I = 0; I < Longitude; ++I)
        {
            int A = J * Longitude + I, B = J * Longitude + (I + 1) % Longitude;
            int C = (J + 1) * Longitude + I, D = (J + 1) * Longitude + (I + 1) % Longitude;
            if (J > 0) Triangle(A, B, C);
            if (J < Latitude - 1) Triangle(B, D, C);
        }
    Mesh->GetStaticMaterials().Add(FStaticMaterial());
    Mesh->SetNumSourceModels(1);
    auto& Build = Mesh->GetSourceModel(0).BuildSettings;
    Build.bRecomputeNormals = false;
    Build.bRecomputeTangents = true;
    Build.bGenerateLightmapUVs = false;
    Mesh->CreateMeshDescription(0, MoveTemp(MD));
    Mesh->CommitMeshDescription(0);
    Mesh->Build(false);
    FStaticMeshCompilingManager::Get().FinishAllCompilation();
    return Mesh;
}
}

UDEMParticleSettingsCommandlet::UDEMParticleSettingsCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 UDEMParticleSettingsCommandlet::Main(const FString& Params)
{
    const bool Create = FParse::Param(*Params, TEXT("CreateDefault"));
    const bool Verify = FParse::Param(*Params, TEXT("Verify"));
    if (Create == Verify)
    {
        UE_LOG(LogTemp, Error, TEXT("Specify exactly one of -CreateDefault or -Verify."));
        return 1;
    }
    const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
    const bool Exists = FPackageName::DoesPackageExist(PackageName);
    UDEMParticleSettings* Asset = nullptr;
    if (Exists)
    {
        Asset = LoadObject<UDEMParticleSettings>(nullptr, *(FString(PackageName) + TEXT(".") + AssetName));
        UE_LOG(LogTemp, Display, TEXT("Existing asset inspected; will not overwrite: %s"), *Filename);
    }
    else if (Create)
    {
        UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
        if (!Mesh) return 2;
        FStaticMeshCompilingManager::Get().FinishAllCompilation();
        UPackage* Package = CreatePackage(PackageName);
        UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
        Factory->DataAssetClass = UDEMParticleSettings::StaticClass();
        Asset = Cast<UDEMParticleSettings>(Factory->FactoryCreateNew(UDEMParticleSettings::StaticClass(), Package,
            AssetName, RF_Public | RF_Standalone, nullptr, GWarn));
        if (!Asset) return 3;
        Asset->SphereMesh = Mesh;
        FDataValidationContext Context;
        if (Asset->IsDataValid(Context) != EDataValidationResult::Valid)
        {
            UE_LOG(LogTemp, Error, TEXT("Default validation failed (%u errors)"), Context.GetNumErrors());
            return 4;
        }
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        if (!UPackage::SavePackage(Package, Asset, *Filename, Args)) return 5;
        UE_LOG(LogTemp, Display, TEXT("Created default settings with UDataAssetFactory: %s"), *Filename);
    }
    if (!Asset)
    {
        UE_LOG(LogTemp, Error, TEXT("Default missing or wrong class; no existing file modified."));
        return 6;
    }
    FStaticMeshCompilingManager::Get().FinishAllCompilation();
    if (Create)
    {
        UE_LOG(LogTemp, Display, TEXT("Current asset state:\n%s"), *Describe(Asset));
        return 0;
    }

    int32 Failures = 0, Checks = 0;
    FString Report = TEXT("DEM particle settings: fresh-process reload and C++ commandlet checks\n");
    auto Check = [&](bool Condition, const FString& Name)
    {
        ++Checks;
        if (!Condition) ++Failures;
        const FString Line = FString::Printf(TEXT("%s %s\n"), Condition ? TEXT("PASS") : TEXT("FAIL"), *Name);
        Report += Line;
        UE_LOG(LogTemp, Display, TEXT("%s"), *Line);
    };
    const FString Before = Describe(Asset);
    const bool DirtyBefore = Asset->GetPackage()->IsDirty();
    const UFunction* PhysicsFunction = Asset->FindFunction(GET_FUNCTION_NAME_CHECKED(UDEMParticleSettings, TryCreatePhysicsSnapshot));
    const UFunction* RenderFunction = Asset->FindFunction(GET_FUNCTION_NAME_CHECKED(UDEMParticleSettings, TryCreateRenderSnapshot));
    Check(PhysicsFunction && RenderFunction && PhysicsFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable) &&
        RenderFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable) && !PhysicsFunction->HasAnyFunctionFlags(FUNC_BlueprintPure) &&
        !RenderFunction->HasAnyFunctionFlags(FUNC_BlueprintPure), TEXT("snapshot Blueprint nodes have explicit execution pins for generation/reset"));
    Check(Asset->RadiusMeters == .01 && Asset->DensityKgPerCubicMeter == 1000 &&
        Asset->NormalStiffness == 500000 && Asset->TangentialStiffness == 250000 &&
        Asset->NormalDampingRatio == .2 && Asset->TangentialDampingRatio == .2 &&
        Asset->ParticleFriction == .4 && Asset->WallFriction == .4, TEXT("reloaded all default physical inputs"));
    Check(Asset->SphereMesh && Asset->SphereMesh->GetPathName() == MeshPath && !Asset->MaterialOverride,
        TEXT("reloaded sphere mesh reference and optional null material"));
    TArray<FText> Errors;
    FDEMParticlePhysicsSnapshot Base;
    Check(Asset->TryCreatePhysicsSnapshot(Base, Errors) && Errors.IsEmpty(), TEXT("default physics snapshot"));
    Check(Near(Base.Derived.DiameterMeters, .02), TEXT("diameter 0.02 m"));
    Check(Near(Base.Derived.VolumeCubicMeters, 4.188790204786391e-6), TEXT("volume 4*pi*r^3/3"));
    Check(Near(Base.Derived.MassKg, 0.004188790204786391), TEXT("mass 0.004188790204786391 kg"));
    Check(Near(Base.Derived.InertiaKgMetersSquared, 1.6755160819145568e-7), TEXT("inertia 1.6755160819145568e-7 kg*m^2"));
    Report += FString::Printf(TEXT("MEASURED mass=%.17g inertia=%.17g\n"), Base.Derived.MassKg, Base.Derived.InertiaKgMetersSquared);
    FDEMParticleDerivedProperties Derived;
    Check(Asset->GetDerivedProperties(Derived, Errors) && Derived.MassKg == Base.Derived.MassKg, TEXT("on-demand BlueprintPure derived query"));

    UDEMParticleSettings* Copy = DuplicateObject<UDEMParticleSettings>(Asset, GetTransientPackage());
    FDEMParticlePhysicsSnapshot BeforeEdit;
    Check(Copy->TryCreatePhysicsSnapshot(BeforeEdit, Errors), TEXT("snapshot from editable transient source"));
    Copy->RadiusMeters *= 2;
    FDEMParticlePhysicsSnapshot Twice;
    Check(Copy->TryCreatePhysicsSnapshot(Twice, Errors) && Near(Twice.Derived.MassKg / Base.Derived.MassKg, 8) &&
        Near(Twice.Derived.InertiaKgMetersSquared / Base.Derived.InertiaKgMetersSquared, 32), TEXT("double radius: mass x8 / inertia x32"));
    Check(BeforeEdit.RadiusMeters == .01 && Near(BeforeEdit.Derived.MassKg, 0.004188790204786391), TEXT("prior snapshot unaffected by later edits to its source"));
    Copy->RadiusMeters = .01;
    Copy->SphereMesh = nullptr;
    Check(Copy->ValidatePhysics(Errors) && !Copy->ValidateDisplay(Errors), TEXT("headless physics valid; missing display mesh rejected"));
    FDataValidationContext HeadlessContext;
    Check(Copy->IsDataValid(HeadlessContext) == EDataValidationResult::Valid && HeadlessContext.GetNumWarnings() > 0,
        TEXT("Unreal asset validation: missing mesh is a warning for headless asset"));

    struct Field { const TCHAR* Name; double UDEMParticleSettings::* Member; bool Positive; };
    const Field Fields[] = {
        {TEXT("RadiusMeters"), &UDEMParticleSettings::RadiusMeters, true},
        {TEXT("DensityKgPerCubicMeter"), &UDEMParticleSettings::DensityKgPerCubicMeter, true},
        {TEXT("NormalStiffness"), &UDEMParticleSettings::NormalStiffness, true},
        {TEXT("TangentialStiffness"), &UDEMParticleSettings::TangentialStiffness, true},
        {TEXT("NormalDampingRatio"), &UDEMParticleSettings::NormalDampingRatio, false},
        {TEXT("TangentialDampingRatio"), &UDEMParticleSettings::TangentialDampingRatio, false},
        {TEXT("ParticleFriction"), &UDEMParticleSettings::ParticleFriction, false},
        {TEXT("WallFriction"), &UDEMParticleSettings::WallFriction, false}};
    for (const Field& F : Fields)
    {
        const double Original = Copy->*(F.Member);
        const double Invalid[] = {-1, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
        for (int I = 0; I < 4; ++I)
        {
            Copy->*(F.Member) = Invalid[I];
            FDEMParticlePhysicsSnapshot Bad = Base;
            const bool Rejected = !Copy->TryCreatePhysicsSnapshot(Bad, Errors);
            Check(Rejected && !Errors.IsEmpty() && Errors[0].ToString().Contains(F.Name) && Bad.Derived.MassKg == 0,
                FString::Printf(TEXT("%s rejects invalid case %d with field error; clears output"), F.Name, I));
        }
        Copy->*(F.Member) = 0;
        Check(Copy->ValidatePhysics(Errors) != F.Positive, FString::Printf(TEXT("%s zero policy"), F.Name));
        Copy->*(F.Member) = Original;
    }
    auto Extreme = [&](double Radius, double Density, const TCHAR* Name, const TCHAR* ErrorField)
    {
        Copy->RadiusMeters = Radius;
        Copy->DensityKgPerCubicMeter = Density;
        const bool Rejected = !Copy->ValidatePhysics(Errors);
        bool Found = false;
        for (const FText& E : Errors) Found |= E.ToString().Contains(ErrorField);
        Check(Rejected && Found, Name);
        for (const FText& E : Errors) Report += TEXT("  expected error: ") + E.ToString() + TEXT("\n");
    };
    Extreme(1.e200, 1000, TEXT("volume overflow"), TEXT("VolumeCubicMeters"));
    Extreme(1.e-200, 1000, TEXT("volume underflow"), TEXT("VolumeCubicMeters"));
    Extreme(1, std::numeric_limits<double>::max(), TEXT("mass overflow"), TEXT("MassKg"));
    Extreme(.01, std::numeric_limits<double>::min(), TEXT("mass underflow"), TEXT("MassKg"));
    Copy->RadiusMeters = 1.e80;
    Copy->DensityKgPerCubicMeter = 1.e-200;
    Check(Copy->ValidatePhysics(Errors), TEXT("wide exponent spread with representable derived values"));
    Copy->RadiusMeters = 1.e60;
    Copy->DensityKgPerCubicMeter = 1.e100;
    Check(!Copy->ValidatePhysics(Errors) && Errors.Last().ToString().Contains(TEXT("Inertia")), TEXT("inertia overflow"));
    Extreme(1, 4.e307, TEXT("inverse inertia underflow"), TEXT("InverseInertia"));
    Extreme(1.e-70, 1, TEXT("inertia underflow"), TEXT("InertiaKgMetersSquared"));
    Copy->RadiusMeters = .01;
    Copy->DensityKgPerCubicMeter = 1000;
    Copy->NormalDampingRatio = 2;
    Copy->ParticleFriction = 3;
    FDEMParticlePhysicsSnapshot Unclamped;
    Check(Copy->TryCreatePhysicsSnapshot(Unclamped, Errors) && Unclamped.NormalDampingRatio == 2 && Unclamped.ParticleFriction == 3,
        TEXT("finite damping/friction above one preserved without clamping"));
    Copy->NormalDampingRatio = .2;
    Copy->ParticleFriction = .4;

    FDEMParticleRenderSnapshot Render;
    Check(Asset->TryCreateRenderSnapshot(Render, Errors) && Near(Render.SourceRadiusCm * Render.UniformScale, 1), TEXT("default mesh gives physical 1 cm display radius"));
    Report += FString::Printf(TEXT("MEASURED default source radius cm=%.17g uniform scale=%.17g\n"), Render.SourceRadiusCm, Render.UniformScale);
    for (double SourceRadius : {2.5, 37.0})
    {
        Copy->SphereMesh = MakeSphere(SourceRadius);
        Check(Copy->TryCreateRenderSnapshot(Render, Errors) && Near(Render.SourceRadiusCm, SourceRadius) &&
            Near(Render.SourceRadiusCm * Render.UniformScale, Copy->RadiusMeters * 100),
            FString::Printf(TEXT("generated sphere radius %.1f cm scales to physical radius"), SourceRadius));
        const auto Positions = Copy->SphereMesh->GetMeshDescription(0)->GetVertexPositions();
        bool AllOnSphere = true;
        for (const auto Vertex : Copy->SphereMesh->GetMeshDescription(0)->Vertices().GetElementIDs())
            AllOnSphere &= Near(FVector(Positions[Vertex]).Length() * Render.UniformScale, 1, 1.e-6);
        Check(AllOnSphere, TEXT("generated source vertices land on physical sphere after scale"));
    }
    UStaticMesh* GoodMesh = Copy->SphereMesh;
    Copy->SphereMesh = MakeSphere(2.5, FVector(1, 0, 0));
    Check(!Copy->ValidateDisplay(Errors) && Errors[0].ToString().Contains(TEXT("Center")), TEXT("off-origin mesh rejected"));
    Copy->SphereMesh = MakeSphere(2.5, FVector::ZeroVector, FVector(1, 2, 1));
    Check(!Copy->ValidateDisplay(Errors) && Errors[0].ToString().Contains(TEXT("Extents")), TEXT("nonuniform mesh rejected"));
    FDataValidationContext BadMeshContext;
    Check(Copy->IsDataValid(BadMeshContext) == EDataValidationResult::Invalid && BadMeshContext.GetNumErrors() > 0,
        TEXT("Unreal asset validation rejects assigned invalid mesh"));
    Copy->SphereMesh = NewObject<UStaticMesh>();
    Check(!Copy->ValidateDisplay(Errors), TEXT("empty/unbuilt mesh rejected"));
    Copy->SphereMesh = GoodMesh;
    const auto SavedBounds = GoodMesh->GetRenderData()->Bounds;
    GoodMesh->GetRenderData()->Bounds.BoxExtent = FVector::ZeroVector;
    Check(!Copy->ValidateDisplay(Errors) && Errors[0].ToString().Contains(TEXT("Bounds")), TEXT("zero-size bounds rejected"));
    GoodMesh->GetRenderData()->Bounds = SavedBounds;
    GoodMesh->GetRenderData()->Bounds.Origin.X = std::numeric_limits<double>::quiet_NaN();
    Check(!Copy->ValidateDisplay(Errors), TEXT("nonfinite bounds rejected"));
    GoodMesh->GetRenderData()->Bounds = SavedBounds;
    Copy->RadiusMeters = 1.e100;
    Check(!Copy->ValidateDisplay(Errors), TEXT("float render scale overflow rejected"));
    Copy->RadiusMeters = 1.e-100;
    Check(!Copy->ValidateDisplay(Errors), TEXT("float render scale underflow rejected"));
    Copy->RadiusMeters = .01;
    Copy->SphereMesh = nullptr;
    Render.Physics = Base;
    Check(!Copy->TryCreateRenderSnapshot(Render, Errors) && Render.Physics.Derived.MassKg == 0 && !Render.SphereMesh && Render.UniformScale == 0,
        TEXT("failed render snapshot clears all output"));
    Copy->RadiusMeters = -1;
    FDataValidationContext BadPhysicsContext;
    Check(Copy->IsDataValid(BadPhysicsContext) == EDataValidationResult::Invalid && BadPhysicsContext.GetNumErrors() > 0,
        TEXT("Unreal asset validation rejects invalid physics"));

    FDataValidationContext DefaultContext;
    Check(Asset->IsDataValid(DefaultContext) == EDataValidationResult::Valid && DefaultContext.GetNumErrors() == 0,
        TEXT("default passes Unreal IsDataValid"));
    Check(Before == Describe(Asset) && DirtyBefore == Asset->GetPackage()->IsDirty(), TEXT("source asset properties and package dirty state unchanged"));
    int32 ObjectReferences = 0;
    bool OnlyDisplayObjects = true;
    for (TFieldIterator<FProperty> It(UDEMParticleSettings::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
        if (const auto* ObjectProperty = CastField<FObjectPropertyBase>(*It))
        {
            ++ObjectReferences;
            OnlyDisplayObjects &= It->GetFName() == GET_MEMBER_NAME_CHECKED(UDEMParticleSettings, SphereMesh) ||
                It->GetFName() == GET_MEMBER_NAME_CHECKED(UDEMParticleSettings, MaterialOverride);
        }
    Check(ObjectReferences == 2 && OnlyDisplayObjects && Asset->GetOuter()->IsA<UPackage>(), TEXT("asset owns only mesh/material references; package outer, no actor/world/solver"));
    const FLinkerLoad* Linker = Asset->GetLinker();
    bool OnlyExpectedImports = Linker != nullptr;
    bool HasSphereImport = false;
    if (Linker)
        for (const FObjectImport& Import : Linker->ImportMap)
        {
            if (Import.ClassName == NAME_Package)
            {
                const FString Name = Import.ObjectName.ToString();
                Report += TEXT("PACKAGE IMPORT ") + Name + TEXT("\n");
                HasSphereImport |= Name == TEXT("/Game/DEM/SM_DEMSphere");
                OnlyExpectedImports &= Name.StartsWith(TEXT("/Script/")) || Name == TEXT("/Game/DEM/SM_DEMSphere");
            }
        }
    Check(OnlyExpectedImports && HasSphereImport, TEXT("serialized package imports only script modules and shared sphere; no level/actor asset dependency"));
    Report += FString::Printf(TEXT("TOTAL checks=%d failures=%d\n"), Checks, Failures);
    const FString Output = FPaths::ProjectPluginsDir() / TEXT("DEMPrototype/ParticleSettingsValidation/verification.txt");
    if (!FFileHelper::SaveStringToFile(Report, *Output)) return 7;
    return Failures == 0 ? 0 : 8;
}
