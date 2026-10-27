#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "DEMParticleSettingsCommandlet.generated.h"

/** -run=DEMParticleSettings -CreateDefault OR -Verify. Never opens/saves a map. */
UCLASS()
class UDEMParticleSettingsCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UDEMParticleSettingsCommandlet();
    virtual int32 Main(const FString& Params) override;
};
