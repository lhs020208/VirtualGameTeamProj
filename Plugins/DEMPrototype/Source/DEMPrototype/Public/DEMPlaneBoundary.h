#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "DEMSolver.h"
#include "DEMPlaneBoundary.generated.h"
class ADEMWorld;
class UBoxComponent;
class UArrowComponent;
/** Fixed two-sided finite rectangle. Local X/Y span; +Z normal; no Chaos. */
UCLASS(ClassGroup=(DEM), meta=(BlueprintSpawnableComponent))
class DEMPROTOTYPE_API UDEMPlaneBoundaryComponent : public USceneComponent
{
 GENERATED_BODY()
public:
 UDEMPlaneBoundaryComponent();
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DEM") TObjectPtr<ADEMWorld> TargetSimulation;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DEM", meta=(ClampMin="0",ToolTip="Unique stable boundary ID, 0..2147483647; used for contact history.")) int32 BoundaryID=0;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DEM", meta=(Units="m",ClampMin="0",ToolTip="Full width along local X, meters. Two-sided zero-thickness DEM face.")) double Width=0.3;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DEM", meta=(Units="m",ClampMin="0")) double Height=0.3;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DEM", meta=(ToolTip="Include vertical contact force in support statistic; enable on the floor only.")) bool bMeasureSupport=false;
 bool Snapshot(dem::Plane& Face,FString& Error) const;
protected:
 virtual void BeginPlay() override;
};
UCLASS(Blueprintable)
class DEMPROTOTYPE_API ADEMPlaneBoundary : public AActor
{
 GENERATED_BODY()
public:
 ADEMPlaneBoundary();
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="DEM") TObjectPtr<UDEMPlaneBoundaryComponent> Boundary;
 virtual void OnConstruction(const FTransform& Transform) override;
private:
 UPROPERTY() TObjectPtr<UBoxComponent> Outline;
 UPROPERTY() TObjectPtr<UArrowComponent> Normal;
};
