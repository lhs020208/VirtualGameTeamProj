#pragma once
#include "CoreMinimal.h"
#include "ProcessRecipe.h"
#include "Dom/JsonObject.h"
class UProcessControlService;
namespace ProcessWebCommands { struct FCommand; struct FResult; }
/** Game-thread configuration/recovery adapter. No worker physics or independent sequencer. */
class FProcessWebBatch
{
public:
 FProcessRecipeStore Store;
 TMap<FString,FString> Files;
 FString CheckKey,CheckToken,ExecutionBatch,ExecutionRecipeRevision;
 static bool Handles(const FString& Kind);
 static bool ReadOnly(const FString& Kind);
 static FString RecipeFingerprint(const UProcessControlService& S);
 FString Reason(const FString& Kind,UProcessControlService& S,double Now) const;
 FString RecoveryKey(const UProcessControlService& S) const;
 bool Checked(const UProcessControlService& S) const;
 void Execute(const ProcessWebCommands::FCommand& C,UProcessControlService& S,ProcessWebCommands::FResult& R);
 TSharedPtr<FJsonObject> Describe(UProcessControlService& S,double Now);
 void Observe(UProcessControlService& S,ProcessWebCommands::FResult& R);
};
