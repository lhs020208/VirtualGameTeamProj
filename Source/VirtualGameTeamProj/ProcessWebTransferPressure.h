#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
class UProcessControlService;
namespace ProcessWebTransferPressure
{
 FString Reason(const UProcessControlService& S,double Now);
 // One already-confirmed service snapshot, no worker reads or device commands.
 TSharedPtr<FJsonObject> Describe(const UProcessControlService& S,double Now);
}
