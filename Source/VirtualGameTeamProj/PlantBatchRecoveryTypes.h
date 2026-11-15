#pragma once
#include "CoreMinimal.h"
#include "PlantBatchRecoveryTypes.generated.h"

USTRUCT(BlueprintType)
struct FPlantRecoveryMaterial
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) int32 Supplier=-1;
 UPROPERTY(BlueprintReadOnly) int32 Source=0;
 UPROPERTY(BlueprintReadOnly) int32 SupplyConnector=0;
 UPROPERTY(BlueprintReadOnly) int32 Weighing=0;
 UPROPERTY(BlueprintReadOnly) int32 Pipe=0;
 UPROPERTY(BlueprintReadOnly) int32 Receiver=0;
 UPROPERTY(BlueprintReadOnly) int32 MixerConnector=0;
 UPROPERTY(BlueprintReadOnly) int32 Mixer=0;
 UPROPERTY(BlueprintReadOnly) int32 Unknown=0;
 UPROPERTY(BlueprintReadOnly) TArray<int32> SelectedIds;
 UPROPERTY(BlueprintReadOnly) TArray<int32> PassedIds;
 UPROPERTY(BlueprintReadOnly) TArray<int32> ReceiverIds;
 UPROPERTY(BlueprintReadOnly) TArray<int32> MixerIds;
};

// Worker-owned identity proof. A stop revokes permission, never erases contamination or cohorts.
USTRUCT(BlueprintType)
struct FPlantBatchRecoverySnapshot
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) bool Supported=false;
 UPROPERTY(BlueprintReadOnly) bool Valid=false;
 UPROPERTY(BlueprintReadOnly) int64 AcceptedRevision=0;
 UPROPERTY(BlueprintReadOnly) int64 RejectedRevision=0;
 UPROPERTY(BlueprintReadOnly) int64 Ticket=0;
 UPROPERTY(BlueprintReadOnly) FGuid RequestId;
 UPROPERTY(BlueprintReadOnly) FString Reason;
 UPROPERTY(BlueprintReadOnly) double ObservedSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double AcceptedSeconds=-1;
 UPROPERTY(BlueprintReadOnly) double AutomaticMixSeconds=0;
 UPROPERTY(BlueprintReadOnly) TArray<FPlantRecoveryMaterial> Materials;
 UPROPERTY(BlueprintReadOnly) TArray<int32> ForeignIds;
 UPROPERTY(BlueprintReadOnly) TArray<int32> ForeignSuppliers;
 UPROPERTY(BlueprintReadOnly) TArray<FString> ForeignRegions;
};
