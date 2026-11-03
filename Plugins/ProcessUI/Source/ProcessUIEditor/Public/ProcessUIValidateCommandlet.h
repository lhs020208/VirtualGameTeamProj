#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ProcessUIValidateCommandlet.generated.h"
UCLASS()
class UProcessUIValidateCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UProcessUIValidateCommandlet();
    int32 Main(const FString& Params) override;
};
