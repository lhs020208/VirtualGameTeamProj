#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DEMSolver.h"
#include "DEMParticleSettings.h"
#include "DEMWorld.generated.h"
class ADEMEmitter;
class UDEMPlaneBoundaryComponent;
UCLASS(Blueprintable)
class DEMPROTOTYPE_API ADEMWorld : public AActor
{
 GENERATED_BODY()
public:
 ADEMWorld();
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Physics",meta=(ToolTip="Unreal world axes, m/s^2. Manager transform is ignored.")) FVector Gravity=FVector(0,0,-9.81);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Physics",meta=(ClampMin="0",Units="s")) double FixedDt=0.00001;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Clock",meta=(ClampMin="0")) double SpeedMultiplier=1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Clock",meta=(ClampMin="1",ClampMax="10000")) int32 MaxSubsteps=500;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Clock",meta=(ClampMin="0",Units="s")) double MaximumLag=2;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Display",meta=(ClampMin="1",ClampMax="240")) double DisplayHz=30;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DEM|Clock") bool bAutoStart=true;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category="DEM|Status") FString Status=TEXT("Waiting for BeginPlay registration.");
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category="DEM|Status") bool bRunning=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category="DEM|Status") bool bResetRequired=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category="DEM|Status") double SimulationTime=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category="DEM|Status") double Lag=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category="DEM|Status") int32 ParticleCount=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category="DEM|Status") int32 EmitterCount=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category="DEM|Status") int32 BoundaryCount=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category="DEM|Status") int32 FrameSubsteps=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category="DEM|Status") int64 ContactCount=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category="DEM|Status") int64 CandidatePairs=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category="DEM|Status") double MaximumSpeed=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category="DEM|Status") double KineticEnergy=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category="DEM|Status") double MaximumOverlapRatio=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category="DEM|Status") double StepMilliseconds=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Transient,Category="DEM|Status") bool bNonFiniteOrFailed=false;
 UFUNCTION(BlueprintCallable,CallInEditor,Category="DEM") void Start();
 UFUNCTION(BlueprintCallable,CallInEditor,Category="DEM") void Pause();
 UFUNCTION(BlueprintCallable,CallInEditor,Category="DEM") void Step();
 UFUNCTION(BlueprintCallable,CallInEditor,Category="DEM") void Reset();
 UFUNCTION(BlueprintCallable,Category="DEM",meta=(ToolTip="Persistent load until replaced or Reset. Unreal world polar force N and axial torque N*m; applied at next fixed step. Not CFD coupling."))
 bool SetExternalLoad(int32 GlobalID,FVector ForceN,FVector TorqueNm);
 UFUNCTION(BlueprintCallable,Category="DEM") void ClearExternalLoads();
 void RegisterEmitter(ADEMEmitter* E);
 void RegisterBoundary(UDEMPlaneBoundaryComponent* B);
 static ADEMWorld* Resolve(UWorld* World,ADEMWorld* Explicit,FString& Error);
 void Invalidate(const FString& Reason);
 void Advance(double FrameSeconds);
 const dem::Solver& GetSolver() const {return Solver;}
 bool IsReady() const {return bReady;}
 virtual void Tick(float DeltaSeconds) override;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 struct Source { TWeakObjectPtr<ADEMEmitter> E; int32 First=0,Count=0,Order=0; };
 struct FaceSource {TWeakObjectPtr<UDEMPlaneBoundaryComponent> B;dem::Plane Initial;};
 TArray<Source> Sources;
 TArray<FaceSource> Faces;
 TArray<TWeakObjectPtr<ADEMEmitter>> PendingEmitters;
 TArray<TWeakObjectPtr<UDEMPlaneBoundaryComponent>> PendingBoundaries;
 dem::Solver Solver;
 bool bReady=false,bInitialAttempt=false;
 double DisplayClock=0;
 uint64 LastUploadFrame=MAX_uint64;
 bool Initialize();
 bool CheckTopology();
 bool CheckClock();
 bool OneStep();
 void Publish(bool Force=false);
 void UpdateStats();
};
