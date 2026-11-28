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
 // Empty for a legacy recipe: the next charge then uses radii from its input file.
 UPROPERTY(BlueprintReadOnly) TArray<double> DiameterMm;
 // Zero selects a mass target. Positive values are exact particle counts by supplier.
 UPROPERTY(BlueprintReadOnly) TArray<int32> TargetCounts;
 UPROPERTY(BlueprintReadOnly) TArray<double> SupplierFeedPa;
 UPROPERTY(BlueprintReadOnly) FVector FeedPressurePa=FVector(500,-500,0);
 UPROPERTY(BlueprintReadOnly) FVector TransferPressurePa=FVector(0,500,-1000);
};

namespace ProcessRecipe
{
 constexpr int32 Version=2;
 inline double GrainKg(double Mm){
  const double R=static_cast<float>(Mm*0.0005); // same float radius sent to OpenCL
  return (4.0/3.0)*UE_DOUBLE_PI*1200.0*R*R*R;
 }
 inline bool ValidDiameter(double Mm){return FMath::IsFinite(Mm)&&Mm>=2&&Mm<20&&static_cast<float>(Mm*.0005)<.01f;}

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
