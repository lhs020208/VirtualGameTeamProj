#pragma once
#include "CoreMinimal.h"
class UButton;

// Presentation only. Never use these rounded strings as control or persistence values.
namespace ProcessDisplay
{
 FString Number(double Value, int32 Decimals=2);
 FString Mass(double Kg, bool Valid=true, int32 Particles=INDEX_NONE);
 FString Exact(double Value);
 bool Parse(const FString& Text, double& Value);
 FString CompactText(const FString& Text);
 void MouseOnly(UButton* Button);
}
