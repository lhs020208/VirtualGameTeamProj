#include "DEMParticleSettings.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "StaticMeshResources.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include <cmath>
#include <initializer_list>
#include <limits>

namespace
{
void AddSettingsError(TArray<FText>& Errors, const TCHAR* Field, const TCHAR* Reason)
{
    Errors.Add(FText::FromString(FString::Printf(TEXT("%s: %s"), Field, Reason)));
}

bool CheckInput(double Value, const TCHAR* Field, bool Positive, TArray<FText>& Errors)
{
    if (!std::isfinite(Value) || (Positive ? Value <= 0 : Value < 0))
    {
        AddSettingsError(Errors, Field, Positive ? TEXT("must be finite and > 0.") : TEXT("must be finite and >= 0."));
        return false;
    }
    return true;
}

// Scale mantissas separately so an intermediate product cannot overflow/underflow
// when the final result is representable (MSVC long double is only 64 bits).
bool Product(std::initializer_list<double> Factors, double& Out, const TCHAR* Field, TArray<FText>& Errors)
{
    double Mantissa = 1;
    int Exponent = 0;
    for (double Factor : Factors)
    {
        int E = 0;
        Mantissa *= std::frexp(Factor, &E);
        Exponent += E;
    }
    Out = std::ldexp(Mantissa, Exponent);
    if (!std::isnormal(Out) || Out <= 0)
    {
        AddSettingsError(Errors, Field, TEXT("overflow or underflow: result must be a positive normal double (check RadiusMeters/DensityKgPerCubicMeter)."));
        return false;
    }
    return true;
}
}

bool UDEMParticleSettings::TryCreatePhysicsSnapshot(FDEMParticlePhysicsSnapshot& Snapshot, TArray<FText>& Errors) const
{
    Snapshot = {};
    Errors.Reset();
    CheckInput(RadiusMeters, TEXT("RadiusMeters"), true, Errors);
    CheckInput(DensityKgPerCubicMeter, TEXT("DensityKgPerCubicMeter"), true, Errors);
    CheckInput(NormalStiffness, TEXT("NormalStiffness"), true, Errors);
    CheckInput(TangentialStiffness, TEXT("TangentialStiffness"), true, Errors);
    CheckInput(NormalDampingRatio, TEXT("NormalDampingRatio"), false, Errors);
    CheckInput(TangentialDampingRatio, TEXT("TangentialDampingRatio"), false, Errors);
    CheckInput(ParticleFriction, TEXT("ParticleFriction"), false, Errors);
    CheckInput(WallFriction, TEXT("WallFriction"), false, Errors);
    if (!Errors.IsEmpty()) return false;

    FDEMParticleDerivedProperties D;
    Product({2, RadiusMeters}, D.DiameterMeters, TEXT("DiameterMeters"), Errors);
    const bool VolumeOK = Product({4.0 * UE_DOUBLE_PI / 3.0, RadiusMeters, RadiusMeters, RadiusMeters},
        D.VolumeCubicMeters, TEXT("VolumeCubicMeters"), Errors);
    if (VolumeOK && Product({DensityKgPerCubicMeter, D.VolumeCubicMeters}, D.MassKg, TEXT("MassKg"), Errors))
    {
        D.InverseMass = 1.0 / D.MassKg;
        if (!std::isnormal(D.InverseMass)) AddSettingsError(Errors, TEXT("InverseMass"), TEXT("MassKg reciprocal overflows or underflows."));
        if (Product({0.4, D.MassKg, RadiusMeters, RadiusMeters}, D.InertiaKgMetersSquared, TEXT("InertiaKgMetersSquared"), Errors))
        {
            D.InverseInertia = 1.0 / D.InertiaKgMetersSquared;
            if (!std::isnormal(D.InverseInertia)) AddSettingsError(Errors, TEXT("InverseInertia"), TEXT("InertiaKgMetersSquared reciprocal overflows or underflows."));
        }
    }
    if (!Errors.IsEmpty()) return false;

    Snapshot.RadiusMeters = RadiusMeters;
    Snapshot.DensityKgPerCubicMeter = DensityKgPerCubicMeter;
    Snapshot.NormalStiffness = NormalStiffness;
    Snapshot.TangentialStiffness = TangentialStiffness;
    Snapshot.NormalDampingRatio = NormalDampingRatio;
    Snapshot.TangentialDampingRatio = TangentialDampingRatio;
    Snapshot.ParticleFriction = ParticleFriction;
    Snapshot.WallFriction = WallFriction;
    Snapshot.Derived = D;
    return true;
}

bool UDEMParticleSettings::ValidatePhysics(TArray<FText>& Errors) const
{
    FDEMParticlePhysicsSnapshot Snapshot;
    return TryCreatePhysicsSnapshot(Snapshot, Errors);
}

bool UDEMParticleSettings::GetDerivedProperties(FDEMParticleDerivedProperties& Properties, TArray<FText>& Errors) const
{
    FDEMParticlePhysicsSnapshot Snapshot;
    const bool Valid = TryCreatePhysicsSnapshot(Snapshot, Errors);
    Properties = Snapshot.Derived;
    return Valid;
}

bool UDEMParticleSettings::CalculateDisplayScale(double& SourceRadiusCm, double& UniformScale, TArray<FText>& Errors) const
{
    SourceRadiusCm = UniformScale = 0;
    Errors.Reset();
    if (!IsInGameThread())
    {
        AddSettingsError(Errors, TEXT("SphereMesh"), TEXT("display validation must run on the game thread."));
        return false;
    }
    if (!SphereMesh)
    {
        AddSettingsError(Errors, TEXT("SphereMesh"), TEXT("missing; required for display, not for physics-only snapshots."));
        return false;
    }
    const FStaticMeshRenderData* Data = SphereMesh->GetRenderData();
    if (!Data || Data->LODResources.IsEmpty() || Data->LODResources[0].GetNumVertices() == 0)
    {
        AddSettingsError(Errors, TEXT("SphereMesh"), TEXT("no built LOD0 geometry; finish mesh compilation before validation."));
        return false;
    }
    // Render-data bounds exclude user-specified positive/negative bounds extensions.
    const FVector Extent(Data->Bounds.BoxExtent);
    const FVector Center(Data->Bounds.Origin);
    if (!std::isfinite(Extent.X) || !std::isfinite(Extent.Y) || !std::isfinite(Extent.Z) ||
        !std::isfinite(Center.X) || !std::isfinite(Center.Y) || !std::isfinite(Center.Z) ||
        Extent.GetMin() <= 0)
    {
        AddSettingsError(Errors, TEXT("SphereMesh.Bounds"), TEXT("extents must be finite and > 0; center must be finite."));
        return false;
    }
    const double Radius = Extent.GetMax();
    if (Center.GetAbsMax() / Radius > 1.e-4)
        AddSettingsError(Errors, TEXT("SphereMesh.Center"), TEXT("must be at the origin (relative tolerance 0.0001 of source radius)."));
    if ((Radius - Extent.GetMin()) / Radius > 0.01)
        AddSettingsError(Errors, TEXT("SphereMesh.Extents"), TEXT("nonuniform outer dimensions (relative tolerance 1%); use an unscaled sphere."));
    if (!CheckInput(RadiusMeters, TEXT("RadiusMeters"), true, Errors)) return false;
    const double RadiusCm = RadiusMeters * 100.0;
    const double Scale = RadiusCm / Radius;
    // Instance rendering ultimately uses float transforms; do not accept a scale
    // that silently becomes zero/infinite at that boundary.
    const float RenderScale = static_cast<float>(Scale);
    if (!std::isnormal(RadiusCm) || !std::isnormal(Scale) || !std::isnormal(RenderScale) || Scale <= 0 ||
        RadiusCm > std::numeric_limits<float>::max() || RadiusCm < std::numeric_limits<float>::min())
        AddSettingsError(Errors, TEXT("DisplayScale"), TEXT("m-to-cm conversion or uniform float render scale overflows/underflows."));
    if (!Errors.IsEmpty()) return false;
    SourceRadiusCm = Radius;
    UniformScale = Scale;
    return true;
}

bool UDEMParticleSettings::ValidateDisplay(TArray<FText>& Errors) const
{
    double Radius, Scale;
    return CalculateDisplayScale(Radius, Scale, Errors);
}

bool UDEMParticleSettings::TryCreateRenderSnapshot(FDEMParticleRenderSnapshot& Snapshot, TArray<FText>& Errors) const
{
    Snapshot = {};
    FDEMParticleRenderSnapshot Candidate;
    const bool PhysicsOK = TryCreatePhysicsSnapshot(Candidate.Physics, Errors);
    TArray<FText> DisplayErrors;
    const bool DisplayOK = CalculateDisplayScale(Candidate.SourceRadiusCm, Candidate.UniformScale, DisplayErrors);
    Errors.Append(DisplayErrors);
    if (!PhysicsOK || !DisplayOK) return false;
    Candidate.SphereMesh = SphereMesh;
    Candidate.MaterialOverride = MaterialOverride;
    Snapshot = Candidate;
    return true;
}

#if WITH_EDITOR
EDataValidationResult UDEMParticleSettings::IsDataValid(FDataValidationContext& Context) const
{
    const EDataValidationResult Parent = Super::IsDataValid(Context);
    TArray<FText> PhysicsErrors, DisplayErrors;
    const bool PhysicsOK = ValidatePhysics(PhysicsErrors);
    const bool DisplayOK = ValidateDisplay(DisplayErrors);
    for (const FText& E : PhysicsErrors) Context.AddError(E);
    // A physics-only asset is valid without a mesh; an assigned but broken mesh is invalid.
    for (const FText& E : DisplayErrors)
    {
        if (SphereMesh) Context.AddError(E);
        else Context.AddWarning(E);
    }
    return Parent == EDataValidationResult::Invalid || !PhysicsOK || (SphereMesh && !DisplayOK)
        ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif
