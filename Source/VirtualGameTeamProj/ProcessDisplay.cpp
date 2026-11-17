#include "ProcessDisplay.h"
#include "Internationalization/Regex.h"
#include "Components/Button.h"
#include "UObject/UnrealType.h"
#include <charconv>
#include <cstdlib>

void ProcessDisplay::MouseOnly(UButton* B)
{
 if(!B)return;
 if(auto* P=FindFProperty<FBoolProperty>(B->GetClass(),TEXT("IsFocusable")))P->SetPropertyValue_InContainer(B,false);
}

FString ProcessDisplay::Exact(double V)
{
 if(!FMath::IsFinite(V))return FString();
 char Buffer[64];const auto R=std::to_chars(Buffer,Buffer+63,V);
 if(R.ec!=std::errc())return FString::Printf(TEXT("%.17g"),V);
 *R.ptr=0;return UTF8_TO_TCHAR(Buffer);
}
bool ProcessDisplay::Parse(const FString& T,double& V)
{
 const FString Trim=T.TrimStartAndEnd();FTCHARToUTF8 B(*Trim);char* End=nullptr;
 const double N=std::strtod(B.Get(),&End);
 if(Trim.IsEmpty()||End==B.Get()||!End||*End||!FMath::IsFinite(N))return false;
 V=N;return true;
}
FString ProcessDisplay::Number(double V,int32 Digits)
{
 if(!FMath::IsFinite(V))return TEXT("미확인");
 Digits=FMath::Clamp(Digits,0,6);
 const double Quantum=FMath::Pow(10.,-Digits);
 if(V!=0&&FMath::Abs(V)<Quantum)
  return (V<0?TEXT(">-"):TEXT("<"))+FString::Printf(TEXT("%.*f"),Digits,Quantum);
 FString T=FString::Printf(TEXT("%.*f"),Digits,V);
 if(T.Contains(TEXT("."))){while(T.EndsWith(TEXT("0")))T.LeftChopInline(1);if(T.EndsWith(TEXT(".")))T.LeftChopInline(1);}
 return T==TEXT("-0")?FString(TEXT("0")):T;
}
FString ProcessDisplay::Mass(double Kg,bool Valid,int32 Count)
{
 if(!Valid||!FMath::IsFinite(Kg)||Kg<0)return TEXT("미확인");
 FString T=Kg>=1?Number(Kg)+TEXT(" kg"):Number(Kg*1000)+TEXT(" g");
 if(Count>0&&Kg<.00001)T+=FString::Printf(TEXT(" · %d개 남음"),Count);
 return T;
}
FString ProcessDisplay::CompactText(const FString& T)
{
 // Only decimal tokens in operator prose. IDs, integers, original diagnostics and inputs are untouched.
 static const FRegexPattern Pattern(TEXT("[+-]?[0-9]+\\.[0-9]+(?:[eE][+-]?[0-9]+)?"));
 FRegexMatcher M(Pattern,T);FString Result;int32 End=0;
 while(M.FindNext()){
  const int32 Begin=M.GetMatchBeginning(),Next=M.GetMatchEnding();double V=0;
  Result+=T.Mid(End,Begin-End);Result+=Parse(T.Mid(Begin,Next-Begin),V)?Number(V):T.Mid(Begin,Next-Begin);End=Next;
 }
 return Result+T.Mid(End);
}
