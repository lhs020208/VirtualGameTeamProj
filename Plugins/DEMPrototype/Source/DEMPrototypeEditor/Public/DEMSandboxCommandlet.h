#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "DEMSandboxCommandlet.generated.h"
UCLASS()
class UDEMSandboxCommandlet : public UCommandlet
{
    GENERATED_BODY()
  public:
    UDEMSandboxCommandlet();
    virtual int32 Main(const FString &Params) override;
};
