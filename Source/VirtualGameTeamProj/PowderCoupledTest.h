#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PowderCoupledTest.generated.h"
class ACameraActor;class UInstancedStaticMeshComponent;struct FCoupledRuntime;
UCLASS()
class VIRTUALGAMETEAMPROJ_API APowderCoupledTest:public AActor {
 GENERATED_BODY()
public:
 APowderCoupledTest();~APowderCoupledTest();
 virtual void BeginPlay()override;virtual void Tick(float DeltaSeconds)override;virtual void EndPlay(const EEndPlayReason::Type)override;
 UPROPERTY(EditAnywhere,Category="Coupled|Setup",meta=(ClampMin="0",ClampMax="6",ToolTip="0=manual; 1=sequential transport; 2=resuspension/filter; 3=blocked; 4=no air; 5=filter no adhesion; 6=partial obstruction. Presets use fixed virtual materials and dt, Reset required.")) int32 Experiment=0;
 UPROPERTY(EditAnywhere,Category="Coupled|Setup",meta=(ClampMin="1",ClampMax="512")) int32 ParticleCount=64;
 UPROPERTY(EditAnywhere,Category="Coupled|Setup",meta=(ToolTip="SI seconds. Acoustic and contact limits are checked; no automatic accuracy claim.")) double FixedDt=0.000005;
 UPROPERTY(EditAnywhere,Category="Coupled|Setup",meta=(ClampMin="200",ClampMax="500")) double GasTemperatureK=293.15;
 UPROPERTY(EditAnywhere,Category="Coupled|Setup",meta=(ToolTip="0=X, 1=Y, 2=Z. Positive pressure drop drives positive-axis gas flow. Reset required.",ClampMin="0",ClampMax="2")) int32 FlowAxis=0;
 UPROPERTY(EditAnywhere,Category="Coupled|Setup") bool PartialObstruction=false;
 UPROPERTY(EditAnywhere,Category="Coupled|Setup",meta=(ClampMin="-1",ClampMax="5",ToolTip="Manual mode: additional whole-face gas/particle opening; -1 disabled; 0/1 X-/+, 2/3 Y-/+, 4/5 Z-/+. Must differ from flow axis. Reset required.")) int32 ExtraPressureFace=-1;
 UPROPERTY(EditAnywhere,Category="Coupled|Setup",meta=(ClampMin="5000",ClampMax="200000",ToolTip="Absolute reservoir pressure at extra opening. Virtual boundary, not calibrated leak geometry.")) double ExtraPressurePa=101325;
 UPROPERTY(EditAnywhere,Category="Coupled|Material",meta=(ClampMin="0")) double PairAdhesionGamma=0;
 UPROPERTY(EditAnywhere,Category="Coupled|Material",meta=(ClampMin="0")) double WallAdhesionGamma=0;
 UPROPERTY(EditAnywhere,Category="Coupled|Material",meta=(ClampMin="0.5",ClampMax="1.2")) double DiameterScale=1;
 UPROPERTY(EditAnywhere,Category="Coupled|Material",meta=(ClampMin="100",ClampMax="10000")) FVector DensitiesKgM3=FVector(900,1500,2400);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Coupled|Control") bool Running=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Coupled|Control",meta=(ClampMin="0.001",ClampMax="1")) double TargetRate=.1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Coupled|Control",meta=(ToolTip="Pa: inlet absolute pressure 101325, outlet 101325-drop. Negative reverses flow. Read live.",ClampMin="-90000",ClampMax="90000")) double PressureDropPa=25;
 UPROPERTY(VisibleAnywhere,Category="Coupled|Status") double PhysicalSeconds=0;
 UPROPERTY(VisibleAnywhere,Category="Coupled|Status") double ActualRate=0;
 UPROPERTY(VisibleAnywhere,Category="Coupled|Status") double GasMassErrorKg=0;
 UPROPERTY(VisibleAnywhere,Category="Coupled|Status") double ParticleMassErrorKg=0;
 UPROPERTY(VisibleAnywhere,Category="Coupled|Status") double ExitedMassKg=0;
 UPROPERTY(VisibleAnywhere,Category="Coupled|Status") double AppliedPressureDropPa=0;
 UPROPERTY(VisibleAnywhere,Category="Coupled|Status") double SuppliedMassKg=0;
 UPROPERTY(VisibleAnywhere,Category="Coupled|Status") double FilterMassKg=0;
 UPROPERTY(VisibleAnywhere,Category="Coupled|Status") FString ExperimentStage;
 UPROPERTY(VisibleAnywhere,Category="Coupled|Status") FString Error;
 UPROPERTY(VisibleAnywhere,Category="Coupled|Status") FString ExportDirectory;
 UFUNCTION(CallInEditor,BlueprintCallable,Category="Coupled") void ResetSimulation();
 UFUNCTION(BlueprintCallable,Category="Coupled") void InjectPulse();
 UFUNCTION(BlueprintCallable,Category="Coupled") void ExportSnapshot();
private:
 UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Groups;
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Walls;
 UPROPERTY() TObjectPtr<ACameraActor> ViewCamera;
 FCoupledRuntime* Runtime=nullptr;double StopAt=0;bool ScreenshotTaken=false;uint64 Generation=0;
 double DisplayUnitsPerMetre=1000;FVector DomainExtentM=FVector(.32,.12,.12);
 void Stop();void Show();void UpdateCamera();
};
