#pragma once
#include "CoreMinimal.h"

// Empty supplier profiles retain the original recipe schema and its validated operating combinations.
namespace ProcessStagePressure
{
inline FString Validate(const FVector& Feed,const FVector& Transfer,const TArray<double>& Suppliers={})
{
 if(!Suppliers.IsEmpty()){
  auto Within=[](double Pa,double Max){return FMath::IsFinite(Pa)&&Pa>=-1000&&Pa<=Max;};
  if(Suppliers.Num()!=6)return TEXT("A~F 공급 압력 설정이 필요합니다.");
  for(double Pa:Suppliers)if(!Within(Pa,1000))return TEXT("공급 압력은 −1~+1 kPa(g) 범위입니다.");
  if(!Within(Feed.X,1000)||!Within(Feed.Y,1000)||!Within(Feed.Z,0)||Transfer.X!=0||!Within(Transfer.Y,1000)||!Within(Transfer.Z,0))
   return TEXT("단계 압력 범위 오류 · 칭량 −1~+1, 수취 −1~0 kPa(g), 칭량→수취 단계의 공급은 0입니다.");
  return FString();
 }
 if(Feed!=FVector(500,-500,0))return TEXT("공급→칭량은 검증된 공급 +0.5 / 칭량 -0.5 / 수취 0 kPa(g) 조합만 지원합니다.");
 if(Transfer.X!=0||Transfer.Y!=500||!FMath::IsFinite(Transfer.Z)||Transfer.Z < -1000||Transfer.Z>0)
  return TEXT("칭량→수취는 공급 0 / 칭량 +0.5 kPa(g), 수취 -1~0 kPa(g)만 지원합니다.");
 return FString();
}
}
