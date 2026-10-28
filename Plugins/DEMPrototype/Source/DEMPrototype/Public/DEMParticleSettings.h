#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DEMParticleSettings.generated.h"

class UStaticMesh;
class UMaterialInterface;

/** Calculated on demand; never serialized as an editable asset cache. SI units. */
USTRUCT(BlueprintType)
struct DEMPROTOTYPE_API FDEMParticleDerivedProperties
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="DEM|Physics") double DiameterMeters = 0;
    UPROPERTY(BlueprintReadOnly, Category="DEM|Physics") double VolumeCubicMeters = 0;
    UPROPERTY(BlueprintReadOnly, Category="DEM|Physics") double MassKg = 0;
    UPROPERTY(BlueprintReadOnly, Category="DEM|Physics") double InertiaKgMetersSquared = 0;
    UPROPERTY(BlueprintReadOnly, Category="DEM|Physics") double InverseMass = 0;
    UPROPERTY(BlueprintReadOnly, Category="DEM|Physics") double InverseInertia = 0;
};

/** Value copy for one generation/reset. No asset, world, actor or solver reference. */
USTRUCT(BlueprintType)
struct DEMPROTOTYPE_API FDEMParticlePhysicsSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="DEM|Physics") double RadiusMeters = 0;
    UPROPERTY(BlueprintReadOnly, Category="DEM|Physics") double DensityKgPerCubicMeter = 0;
    UPROPERTY(BlueprintReadOnly, Category="DEM|Contact") double NormalStiffness = 0;
    UPROPERTY(BlueprintReadOnly, Category="DEM|Contact") double TangentialStiffness = 0;
    UPROPERTY(BlueprintReadOnly, Category="DEM|Contact") double NormalDampingRatio = 0;
    UPROPERTY(BlueprintReadOnly, Category="DEM|Contact") double TangentialDampingRatio = 0;
    UPROPERTY(BlueprintReadOnly, Category="DEM|Contact") double ParticleFriction = 0;
    UPROPERTY(BlueprintReadOnly, Category="DEM|Contact") double WallFriction = 0;
    UPROPERTY(BlueprintReadOnly, Category="DEM|Physics") FDEMParticleDerivedProperties Derived;
};

/** Retain in a UPROPERTY on the future consumer to keep shared render assets alive. */
USTRUCT(BlueprintType)
struct DEMPROTOTYPE_API FDEMParticleRenderSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="DEM") FDEMParticlePhysicsSnapshot Physics;
    UPROPERTY(BlueprintReadOnly, Category="DEM|Display") TObjectPtr<UStaticMesh> SphereMesh = nullptr;
    UPROPERTY(BlueprintReadOnly, Category="DEM|Display") TObjectPtr<UMaterialInterface> MaterialOverride = nullptr;
    UPROPERTY(BlueprintReadOnly, Category="DEM|Display") double SourceRadiusCm = 0;
    UPROPERTY(BlueprintReadOnly, Category="DEM|Display") double UniformScale = 0;
};

/** Shared, level-independent spherical DEM parameters. Read on generation/reset only.
 * Defaults are historical DEM test assumptions, not measured powder properties.
 * Changing radius/stiffness does not guarantee stability with an existing simulation dt.
 * Different assets do not define a mixed-material contact combination rule.
 */
UCLASS(BlueprintType, meta=(DisplayName="DEM Particle Settings"))
class DEMPROTOTYPE_API UDEMParticleSettings : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Provenance") FString SubstanceName = TEXT("Virtual powder");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Provenance") FString PropertySource = TEXT("Uncalibrated test assumptions");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Provenance", meta=(ToolTip="Metadata only; zero means unknown. This does not change computational radius.")) double SourceDiameterMeters = 0;
    // Explicit opt-in: stiffness/damping are not interpreted by the GPU position solver.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GPU", meta=(ToolTip="GPU position approximation. Binary mixer mode supports unequal mass and optional rotation. Stiffness/damping/cohesion are unsupported."))
    bool bGPUApproximation = false;
    bool ValidateGPU(TArray<FText>& Errors) const;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Physics", meta=(ClampMin="0.0", Units="m", ToolTip="Physical sphere radius in meters; must be finite and strictly positive. Changing radius does not guarantee stability with the existing simulation dt. Test assumption, not measured powder data."))
    double RadiusMeters = 0.01;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Physics", meta=(ClampMin="0.0", DisplayName="Density (kg/m^3)", ToolTip="Finite positive density in kg/m^3. Historical DEM test assumption, not measured powder data."))
    double DensityKgPerCubicMeter = 1000;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contact", meta=(ClampMin="0.0", EditCondition="!bGPUApproximation", DisplayName="Normal Stiffness kn (N/m)", ToolTip="Finite positive normal stiffness in N/m. Changing stiffness does not guarantee stability with the existing simulation dt. Test assumption, not measured powder data."))
    double NormalStiffness = 500000;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contact", meta=(ClampMin="0.0", EditCondition="!bGPUApproximation", DisplayName="Tangential Stiffness kt (N/m)", ToolTip="Finite positive tangential stiffness in N/m. Changing stiffness does not guarantee stability with the existing simulation dt. Test assumption, not measured powder data."))
    double TangentialStiffness = 250000;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contact", meta=(EditCondition="!bGPUApproximation", ClampMin="0.0", ToolTip="Dimensionless normal damping ratio, finite and >= 0. No upper clamp; test assumption."))
    double NormalDampingRatio = 0.2;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contact", meta=(EditCondition="!bGPUApproximation", ClampMin="0.0", ToolTip="Dimensionless tangential damping ratio, finite and >= 0. No upper clamp; test assumption."))
    double TangentialDampingRatio = 0.2;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contact", meta=(ClampMin="0.0", ToolTip="Dimensionless particle-particle friction coefficient, finite and >= 0. Binary cross friction is set explicitly on the mixer."))
    double ParticleFriction = 0.4;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Contact", meta=(ClampMin="0.0", ToolTip="Dimensionless particle-wall friction coefficient, finite and >= 0. Test assumption."))
    double WallFriction = 0.4;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Display", meta=(ToolTip="Shared origin-centered sphere mesh, source dimensions in cm. Uniform scale follows the physical radius; no visual multiplier. Bounds validation cannot prove spherical geometry. Changing mesh does not change the analytical sphere."))
    TObjectPtr<UStaticMesh> SphereMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Display", meta=(ToolTip="Optional shared material; null uses mesh materials. Does not change DEM shape or contact physics. Vertex-displacing materials must not distort the sphere."))
    TObjectPtr<UMaterialInterface> MaterialOverride = nullptr;

    /** Errors are replaced each call. No clamping, asset mutation or mesh requirement. */
    UFUNCTION(BlueprintPure, Category="DEM|Validation")
    bool ValidatePhysics(TArray<FText>& Errors) const;

    /** Display-only validation, including physical radius conversion and scale. Game thread. */
    UFUNCTION(BlueprintPure, Category="DEM|Validation")
    bool ValidateDisplay(TArray<FText>& Errors) const;

    /** Failure clears output. Branch on success before consuming it. */
    UFUNCTION(BlueprintPure, Category="DEM|Physics")
    bool GetDerivedProperties(FDEMParticleDerivedProperties& Properties, TArray<FText>& Errors) const;

    /** Call at generation/reset; consume the copy without mutating the shared asset. */
    UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="DEM|Settings")
    bool TryCreatePhysicsSnapshot(FDEMParticlePhysicsSnapshot& Snapshot, TArray<FText>& Errors) const;

    /** Game thread; validates physics and mesh. Failure clears output. No per-particle objects. */
    UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="DEM|Settings")
    bool TryCreateRenderSnapshot(FDEMParticleRenderSnapshot& Snapshot, TArray<FText>& Errors) const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
    bool CalculateDisplayScale(double& SourceRadiusCm, double& UniformScale, TArray<FText>& Errors) const;
};
