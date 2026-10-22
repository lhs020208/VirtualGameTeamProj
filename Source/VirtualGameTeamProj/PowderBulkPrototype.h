#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "powder/bulk_prototype.hpp"
#include <memory>
#include "PowderBulkPrototype.generated.h"
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
UCLASS()
class VIRTUALGAMETEAMPROJ_API APowderBulkPrototype : public AActor {
 GENERATED_BODY()
public:
 APowderBulkPrototype();
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Bulk") bool Running=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Bulk") bool ValveOpen=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Bulk",meta=(ClampMin="-5",ClampMax="5")) double AirSpeed=1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Bulk",meta=(ClampMin="0.01",ClampMax="2")) double TargetRate=.1;
 UPROPERTY(EditAnywhere,Category="Bulk",meta=(ClampMin="1",ClampMax="50")) double TotalKg=30;
 UPROPERTY(EditAnywhere,Category="Bulk",meta=(ClampMin="0",ClampMax="10000")) int32 DisplayCount=1500;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Bulk") double PhysicalSeconds=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Bulk") double ActualRate=0;
 UFUNCTION(CallInEditor,BlueprintCallable,Category="Bulk") void ResetSimulation();
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Bulk") FString Error;
private:
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> GroupA;
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> GroupB;
 UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> GroupC;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Gate;
 std::unique_ptr<powder::BulkPrototype> Model;
 double Debt=0,RunWall=0,ObservedWall=0,LastTickWall=0;
 void Refresh();
};
UCLASS()
class VIRTUALGAMETEAMPROJ_API APowderBulkGameMode : public AGameModeBase {
 GENERATED_BODY()
public: APowderBulkGameMode();
};
