#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "PlantSimulationSnapshot.h"
#include "ProcessAutoTypes.h"

// Read-only projection. No UObject, worker command, actor lookup, or network access.
namespace ProcessWebSnapshot
{
struct FContext
{
 TSharedPtr<FJsonObject> Control;
 TArray<FProcessFeedEntry> AppliedEntries;
 FString PublisherId, RunId;
 int64 PublicationSequence=0;
 FString BindingStatus=TEXT("waiting"), BindingReason, NextAction, ActualActorPath;
 FString StopStage=TEXT("none"), StopReason, BlockReason;
};
FString Serialize(const FContext& Context,const FPlantSimulationSnapshot* Snapshot,
 const FProcessAutoSnapshot* Automatic,double Now);
}
