#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "powder/bulk_process.hpp"
#include <memory>
#include "PowderProcessTwin.generated.h"
class UInstancedStaticMeshComponent;
UCLASS()
class VIRTUALGAMETEAMPROJ_API APowderProcessTwin : public AActor {
 GENERATED_BODY()
public:
 APowderProcessTwin();
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Process") bool Running=true;
 UPROPERTY(EditAnywhere,Category="Process",meta=(ClampMin="1",ClampMax="50")) double TotalKg=30;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Process",meta=(ClampMin="0.1",ClampMax="10")) double TargetRate=.1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Process",meta=(ClampMin="0",ClampMax="120")) double RPM=30;
 UPROPERTY(EditAnywhere,Category="Virtual material",meta=(ClampMin="100",ClampMax="2000")) double BulkDensity=800;
 UPROPERTY(EditAnywhere,Category="Virtual material",meta=(ClampMin="0.01",ClampMax="2")) double DragRelaxationSeconds=.15;
 UPROPERTY(EditAnywhere,Category="Virtual material",meta=(ClampMin="0",ClampMax="2")) double MixingExchange=.08;
 UPROPERTY(EditAnywhere,Category="Virtual material",meta=(ClampMin="0",ClampMax="1")) double DeadZoneFactor=.15;
 UPROPERTY(EditAnywhere,Category="Process setup",meta=(ClampMin="82000",ClampMax="100000")) double VacuumTargetPa=90000;
 UPROPERTY(EditAnywhere,Category="Process setup",meta=(ClampMin="0.1",ClampMax="5")) double SupplyKgPerSecond=2;
 UPROPERTY(EditAnywhere,Category="Process setup",meta=(ClampMin="1",ClampMax="2")) double RawReserveFactor=1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Process") bool SupplyEnabled=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Process") bool TransferValveOpen=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Process") bool PumpEnabled=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Process") bool DumpValveOpen=true;
 UPROPERTY(EditAnywhere,Category="Process",meta=(ClampMin="0",ClampMax="5000")) int32 DisplayBudget=1500;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Process") FString Phase;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Process") FString Error;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Process") double PhysicalSeconds=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Process") double MeasuredRate=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Process") double WeighedKg=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Process") double ReceiverPressurePa=101325;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Process") double MixerKg=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Process") double MixingIndex=0;
 UFUNCTION(CallInEditor,BlueprintCallable,Category="Process") void ResetSimulation();
 bool Complete()const{return Model&&Model->phase==powder::BulkPhase::Complete;}
 int32 InstanceCount()const;
 const powder::BulkProcess* StateForValidation()const{return Model.get();}
private:
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> PowderA;
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> PowderB;
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> PowderC;
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Structure;
 std::unique_ptr<powder::BulkProcess> Model;
 double Debt=0,LastWall=0,ElapsedWall=0,WindowWall=0,WindowPhysical=0,MinWindowRate=1e30,NextSave=0,EndWall=0;
 int32 Frames=0,Rows=0;bool Benchmark=false,TransferShot=false,MixShot=false,ResultSaved=false;
 FString OutputDir;
 FVector Position(std::size_t Node)const;
 void BuildGeometry();void Refresh();void SaveRecord();void SaveResult();
};
