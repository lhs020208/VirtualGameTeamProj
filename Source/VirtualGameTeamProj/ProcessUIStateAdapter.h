#pragma once

#include "CoreMinimal.h"
#include "PlantSimulationSnapshot.h"
#include "Subsystems/WorldSubsystem.h"
#include "ProcessUIStateAdapter.generated.h"

class APowderPlantTest;
class UUserWidget;

/** The only state supplier for one UI instance. It has no simulation command API. */
UCLASS(BlueprintType)
class VIRTUALGAMETEAMPROJ_API UProcessUIStateAdapter : public UObject
{
    GENERATED_BODY()
public:
    bool Attach(UUserWidget* InWidget, APowderPlantTest* InSource);
    void Detach();
    void Refresh();
    UFUNCTION() void HandleStateRequested(FString EquipmentId);
    UFUNCTION(BlueprintCallable, Category="Process UI|Read Only")
    FProcessEquipmentSnapshot ReadEquipment(const FString& EquipmentId);
    static FProcessEquipmentSnapshot BuildEquipment(const FString& Id, const FPlantSimulationSnapshot* State, double Now, double MaxAge = 2.0);
    static void Format(const FProcessEquipmentSnapshot& State, FText& Status, FText& Quantity, FText& Valve);
    UPROPERTY(BlueprintReadOnly, Category="Process UI|Read Only") TMap<FString, FProcessEquipmentSnapshot> Latest;
    bool IsAttached() const;
private:
    TWeakObjectPtr<APowderPlantTest> SourceActor;
    TWeakObjectPtr<UUserWidget> Widget;
    FProcessSnapshotCursor Cursor;
    FDelegateHandle DestructHandle;
    void Push(const FString& Id);
    void WidgetDestructed(UUserWidget* Removed);
};

/** Opens the existing UI in FlowTest without changing/saving its shared map asset. */
UCLASS()
class VIRTUALGAMETEAMPROJ_API UProcessUIStateSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual void Deinitialize() override;
    // Explicit actor reference only; never selects the first actor of a class.
    UFUNCTION(BlueprintCallable, Category="Process UI|Read Only") bool OpenForActor(APowderPlantTest* Actor);
    UFUNCTION(BlueprintCallable, Category="Process UI|Read Only") void Close();
    bool IsReadOnlyScreenActive() const;
    bool IsObservationViewActive() const;
    UFUNCTION() void HandleObservationViewRequested(bool ShowObservation);
    UPROPERTY(BlueprintReadOnly, Transient) TObjectPtr<UProcessUIStateAdapter> Adapter;
    UPROPERTY(BlueprintReadOnly, Transient) TObjectPtr<UUserWidget> StatusWidget;
protected:
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
private:
    bool PendingOpen = false;
    double LastRefresh = 0;
};
