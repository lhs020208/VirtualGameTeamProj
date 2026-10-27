#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "DEMCommonCommandlet.generated.h"
UCLASS()
class UDEMCommonCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UDEMCommonCommandlet();
 virtual int32 Main(const FString& Params) override;
};
