#pragma once
#include "CoreMinimal.h"

// Dosing acceptance is independent of the transfer mass-conservation tolerance.
namespace PlantDose {
inline FString Validate(double Kg,double ToleranceKg,double ParticleKg,double AvailableKg)
{
 if(!FMath::IsFinite(Kg)||Kg<=0||!FMath::IsFinite(ToleranceKg)||ToleranceKg<0||ToleranceKg>=Kg)
  return TEXT("목표량은 양수, 계량 허용 오차는 0 이상 목표량 미만의 유한값이어야 합니다.");
 if(!FMath::IsFinite(ParticleKg)||ParticleKg<=0||!FMath::IsFinite(AvailableKg)||AvailableKg<=0||AvailableKg/ParticleKg>MAX_int32)
  return TEXT("현재 모델의 입자 질량·공급 가능량을 확인할 수 없습니다.");
 if(Kg>AvailableKg+1e-12)return FString::Printf(TEXT("목표 %.9f g은 A 공급 가능량 %.9f g을 초과합니다."),Kg*1000,AvailableKg*1000);
 const double Low=FMath::Max(1.,FMath::CeilToDouble((Kg-ToleranceKg-1e-12)/ParticleKg));
 const double High=FMath::FloorToDouble((FMath::Min(AvailableKg,Kg+ToleranceKg)+1e-12)/ParticleKg);
 if(Low>High)return FString::Printf(TEXT("입자 1개 %.9f g 단위로 목표 ± 허용 오차를 표현할 수 없습니다. 가장 가까운 모델 질량 %.9f g."),ParticleKg*1000,FMath::RoundToDouble(Kg/ParticleKg)*ParticleKg*1000);
 return FString();
}
}
