#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "DEMEmitterCommandlet.generated.h"

UCLASS()
class UDEMEmitterCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UDEMEmitterCommandlet();
    virtual int32 Main(const FString& Params) override;
};
