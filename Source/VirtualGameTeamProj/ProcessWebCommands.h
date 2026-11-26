#pragma once
#include "CoreMinimal.h"
#include "ProcessControlTypes.h"
#include "Dom/JsonObject.h"
#include "ProcessWebBatch.h"
class UProcessControlService;

namespace ProcessWebCommands
{
struct FCommand
{
 FGuid Id,PressureStage;
 FString SupplierId;
 FString Run,Lease,Equipment,Kind,Unit;
 FString RecipeJson,FileId,FileVersion,RecipeRevision,RecipeFingerprint,BatchId,CheckToken;
 bool Confirm=false;
 int64 Epoch=-1,TransferId=0,MixRun=0;
 EProcessCommand Command=EProcessCommand::StopAll;
 double Value=0,InputValue=0;
 FString Fingerprint() const;
};
// Sole kPa(g) -> Pa conversion for incoming web commands. No actor or network access.
bool Decode(const TSharedPtr<FJsonObject>& Json,FCommand& Out,FString& Error);
struct FResult { FCommand Command; FProcessCommandRecord Record; TSharedPtr<FJsonObject> Data; FString BatchId; int64 RecoveryRevision=0; };
class FBridge
{
public:
 TMap<FGuid,FResult> Results;
 FProcessWebBatch Batch;
 void Receive(const TSharedPtr<FJsonObject>& Frame,const FString& Run,UProcessControlService& Service,double Elapsed);
 void Release(UProcessControlService& Service,const FString& Reason);
 TSharedPtr<FJsonObject> Describe(UProcessControlService* Service,bool Enabled,double Now);
};
}
