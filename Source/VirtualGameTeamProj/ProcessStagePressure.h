#pragma once
#include "CoreMinimal.h"

// Existing verified feed/purge regime is retained. Presentation comparisons vary only RH suction.
namespace ProcessStagePressure
{
inline FString Validate(const FVector& Feed,const FVector& Transfer)
{
 if(Feed!=FVector(500,-500,0))return TEXT("공급→칭량은 검증된 공급 +0.5 / 칭량 -0.5 / 수취 0 kPa(g) 조합만 지원합니다.");
 if(Transfer.X!=0||Transfer.Y!=500||!FMath::IsFinite(Transfer.Z)||Transfer.Z < -1000||Transfer.Z>0)
  return TEXT("칭량→수취는 공급 0 / 칭량 +0.5 kPa(g), 수취 -1~0 kPa(g)만 지원합니다.");
 return FString();
}
}
