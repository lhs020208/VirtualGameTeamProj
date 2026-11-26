#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ProcessWebCommands.h"
#include "ProcessWebStateSubsystem.generated.h"
class IHttpRequest;
class UProcessControlService;

/** Opt-in state publisher and explicitly enabled manual command bridge. No network thread UObject access. */
UCLASS()
class VIRTUALGAMETEAMPROJ_API UProcessWebStateSubsystem : public UTickableWorldSubsystem
{
 GENERATED_BODY()
public:
 virtual void OnWorldBeginPlay(UWorld& World) override;
 virtual void Tick(float DeltaTime) override;
 virtual void Deinitialize() override;
 virtual TStatId GetStatId() const override;
protected:
 virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
private:
 bool Enabled=false, HadSource=false, ControlEnabled=false;
 ProcessWebCommands::FBridge Commands;
 TWeakObjectPtr<UProcessControlService> BoundService;
 TWeakObjectPtr<UObject> PublishedSource;
 FString RunId,Token,ServerId;
 int32 Port=0;
 int64 PublishedConnection=-1;
 double LastPublish=0,LastLinkRead=0;
 TSharedPtr<IHttpRequest,ESPMode::ThreadSafe> Pending;
 void ReadLink();
};
