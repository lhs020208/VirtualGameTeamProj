#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DEMParticleSettings.h"
#include "DEMEmitter.generated.h"

class UInstancedStaticMeshComponent;
class UArrowComponent;



/** Contiguous value data, world-space right-handed DEM coordinates, SI, all double.
 * Polar: (UE.x,-UE.y,UE.z); axial: (-UE.x,UE.y,-UE.z).
 * Indices are local to this emitter/burst, NOT global particle IDs.
 */
struct DEMPROTOTYPE_API FDEMEmittedParticle
{
    int32 LocalIndex = 0;
    FVector3d PositionMeters = FVector3d::ZeroVector;
    FVector3d LinearVelocityMetersPerSecond = FVector3d::ZeroVector;
    FVector3d AngularVelocityRadiansPerSecond = FVector3d::ZeroVector;
    FQuat4d Orientation = FQuat4d::Identity;
};

/** Consumer-owned copy; no actor/asset pointers and no solver allocation. */
struct DEMPROTOTYPE_API FDEMEmitterBurst
{
    FDEMParticlePhysicsSnapshot Settings;
    TArray<FDEMEmittedParticle> Particles;
};

/** Burst preparation and ISM display. Standalone is static; GPU integration is pending; no solver is connected. */
UCLASS(Blueprintable, meta=(DisplayName="DEM Emitter"))
class DEMPROTOTYPE_API ADEMEmitter : public AActor
{
    GENERATED_BODY()
public:
    ADEMEmitter();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DEM|GPU", meta=(ClampMin="0",ClampMax="1",ToolTip="GPU display/tracer group: 0 red, 1 blue. Same physical material."))
    int32 GPUGroup = 0;
    void SuppressStaticDisplay(bool Suppress);
    bool BindGPU(AActor* Consumer);
    void ReleaseGPU(AActor* Consumer);
    bool IsGPUValid(AActor* Consumer) const;
    AActor* GetGPUOwner() const { return GPUOwner.Get(); }
    void DisplayGPU(const TArray<FTransform>& WorldTransforms);
    FString ConfigurationSignature() const;
    UPROPERTY(EditAnywhere, Category="DEM|Layout", meta=(ToolTip="Optional explicit local UE-axis positions in meters. Empty uses the grid; nonempty must match ParticleCount. Used for vessel-fitted reference charges."))
    TArray<FVector> InitialLocalPositionsMeters;
    static constexpr int32 MaxParticleCount = 100000;
    static constexpr int32 MaxPreviewCount = 2048;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DEM|Burst")
    TObjectPtr<UDEMParticleSettings> ParticleSettings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DEM|Burst", meta=(ClampMin="0", ClampMax="100000", ToolTip="One burst, 0..100000. Invalid runtime values fail; never silently truncate the real burst."))
    int32 ParticleCount = 200;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DEM|Grid", meta=(ClampMin="1", ClampMax="100000"))
    int32 ColumnsX = 10;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DEM|Grid", meta=(ClampMin="1", ClampMax="100000"))
    int32 ColumnsY = 10;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DEM|Grid", meta=(ClampMin="0", Units="m", ToolTip="Surface-to-surface gap in meters. Pitch = 2*radius + gap. First layer center is at local Z=radius, X/Y centered on the full ColumnsX/ColumnsY rectangle."))
    double SurfaceGap = 0.001;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DEM|Initial State", meta=(ToolTip="Emitter-local Unreal axes, m/s. Rotated to world and converted to right-handed DEM polar vector. Stored only; no motion this version."))
    FVector InitialLinearVelocity = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DEM|Initial State", meta=(ToolTip="Emitter-local axial vector, rad/s. Rotate to Unreal world then map (-X,+Y,-Z) to DEM. Stored only; no rotation step this version."))
    FVector InitialAngularVelocity = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DEM|Generation")
    bool bGenerateOnBeginPlay = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DEM|Generation", meta=(ToolTip="Editor-only initial layout, at most 2048 displayed. Use Refresh Preview after changing the shared settings asset. No solver data is exported from preview."))
    bool bEditorPreview = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="DEM|Status")
    FString GenerationStatus;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="DEM|Status")
    int32 GeneratedCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="DEM|Status")
    int32 VisibleInstanceCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="DEM|Status")
    bool bPreviewOnly = false;

    UFUNCTION(BlueprintCallable, CallInEditor, Category="DEM|Generation")
    void Generate();
    UFUNCTION(BlueprintCallable, Category="DEM|Generation")
    bool TryGenerate();
    UFUNCTION(BlueprintCallable, CallInEditor, Category="DEM|Generation")
    void Clear();
    UFUNCTION(BlueprintCallable, CallInEditor, Category="DEM|Generation")
    void RefreshPreview();

    /** Game thread. False for preview/cleared/failed burst and clears OutBurst.
     * Generate replaces the local index namespace. Future solvers assign global IDs.
     * Copy ownership is independent; solver lifetime/retirement policy is not implemented.
     */
    bool CopyGeneratedBurst(FDEMEmitterBurst& OutBurst) const;
    const UInstancedStaticMeshComponent* GetParticleInstances() const { return Instances; }

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Destroyed() override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY(VisibleAnywhere, Category="DEM|Components")
    TObjectPtr<USceneComponent> EmitterRoot;
    UPROPERTY(VisibleAnywhere, Transient, Category="DEM|Components")
    TObjectPtr<UInstancedStaticMeshComponent> Instances;
#if WITH_EDITORONLY_DATA
    UPROPERTY()
    TObjectPtr<UArrowComponent> Direction;
#endif
    UPROPERTY(Transient, DuplicateTransient)
    FDEMParticleRenderSnapshot ActiveSettings;
    TArray<FDEMEmittedParticle> Particles;
    bool bValidBurst = false;
    bool bStaticDisplaySuppressed = false;
    TWeakObjectPtr<AActor> GPUOwner;
    bool bGPUInvalidated=false;
    FString BoundSignature;
    bool BuildBurst(bool bPreview);
    bool Fail(const FString& Reason);
};
