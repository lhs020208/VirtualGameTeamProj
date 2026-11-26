#pragma once
#include "CoreMinimal.h"
#include "ProcessAutoTypes.h"
#include "ProcessRecipe.generated.h"

/** Configuration only. No worker state, commands, particle IDs or material physics. */
USTRUCT(BlueprintType)
struct FProcessRecipe
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) TArray<FProcessFeedEntry> Entries;
 UPROPERTY(BlueprintReadOnly) FString Name;
 UPROPERTY(BlueprintReadOnly) EProcessBatchMode Mode=EProcessBatchMode::Full;
 UPROPERTY(BlueprintReadOnly) FString MaterialA=TEXT("원료 A");
 UPROPERTY(BlueprintReadOnly) FString MaterialB=TEXT("원료 B");
 UPROPERTY(BlueprintReadOnly) FString MaterialC=TEXT("첨가제 C");
 UPROPERTY(BlueprintReadOnly) double TargetAKg=0;
 UPROPERTY(BlueprintReadOnly) double ToleranceAKg=0;
 UPROPERTY(BlueprintReadOnly) double TargetBKg=0;
 UPROPERTY(BlueprintReadOnly) double ToleranceBKg=0;
 UPROPERTY(BlueprintReadOnly) bool AdditiveEnabled=false;
 UPROPERTY(BlueprintReadOnly) double TargetCKg=0;
 UPROPERTY(BlueprintReadOnly) double ToleranceCKg=0;
 UPROPERTY(BlueprintReadOnly) bool Scheduled=false;
 UPROPERTY(BlueprintReadOnly) double DelaySeconds=0;
 UPROPERTY(BlueprintReadOnly) double TargetRPM=0;
 UPROPERTY(BlueprintReadOnly) double MixingSeconds=0;
 UPROPERTY(BlueprintReadOnly) TArray<double> SupplierFeedPa;
 UPROPERTY(BlueprintReadOnly) FVector FeedPressurePa=FVector(500,-500,0);
 UPROPERTY(BlueprintReadOnly) FVector TransferPressurePa=FVector(0,500,-1000);
};

namespace ProcessRecipe
{
 constexpr int32 Version=2;
 FString ValidateEntries(const TArray<FProcessFeedEntry>& Entries,bool WithAdditive);
 FString Validate(const FProcessRecipe& R);
 FString Encode(const FProcessRecipe& R);
 // Failed decode never changes Out. Strict version/units/IDs/order and finite numbers.
 bool Decode(const FString& Json,FProcessRecipe& Out,FString& Error);
 bool Same(const FProcessRecipe& A,const FProcessRecipe& B);
 FString Summary(const FProcessRecipe& R);
}

struct FProcessRecipeFile
{
 FString File,Name,Error,Version;
};

/** Same-directory temporary write followed by atomic rename; never delete the old recipe first. */
class FProcessRecipeStore
{
public:
 explicit FProcessRecipeStore(FString Directory=FString());
 FString Root;
 TArray<FProcessRecipeFile> List() const;
 bool Load(const FString& File,FProcessRecipe& Out,FString& Error) const;
 bool SaveNew(const FProcessRecipe& R,FString& SavedFile,FString& Error) const;
 bool ReplaceSelected(const FString& File,const FProcessRecipe& R,FString& Error,const FString& ExpectedVersion=FString()) const;
 FString FileVersion(const FString& File) const;
private:
 bool Commit(const FString& File,const FProcessRecipe& R,bool Replace,FString& Error,const FString& ExpectedVersion=FString()) const;
 bool SafeFile(const FString& File) const;
};
