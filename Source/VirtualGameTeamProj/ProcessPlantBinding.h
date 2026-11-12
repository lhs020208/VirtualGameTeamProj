#pragma once

#include "CoreMinimal.h"

// These bindings were checked against each saved level and its runtime.plant gate order.
// Resolving an exact object path is intentional: never choose the first actor by name/class.
namespace ProcessPlantBinding
{
inline const TCHAR* LowPolyMap = TEXT("/Game/ProcessPlant_20261111_LowPoly/L_ProcessPlant_LowPoly_1111");
inline const TCHAR* FlowTestMap = TEXT("/Game/ProcessPlant_20261029/L_ProcessPlant_FlowTest");

inline FString SourcePath(const FString& Map)
{
    if(Map == LowPolyMap)
        return TEXT("/Game/ProcessPlant_20261111_LowPoly/L_ProcessPlant_LowPoly_1111.L_ProcessPlant_LowPoly_1111:PersistentLevel.PowderPlantTest_0");
    if(Map == FlowTestMap)
        return TEXT("/Game/ProcessPlant_20261029/L_ProcessPlant_FlowTest.L_ProcessPlant_FlowTest:PersistentLevel.PowderPlantTest_0");
    return FString();
}

inline bool IsLowPoly(const FString& Source)
{
    return Source.Contains(TEXT("/ProcessPlant_20261111_LowPoly/"));
}

inline int32 SupplierIndex(const FString& Id)
{
    const TCHAR* Ids[] = {TEXT("supply-a"), TEXT("supply-b"), TEXT("supply-c"),
                         TEXT("supply-d"), TEXT("supply-e"), TEXT("supply-f")};
    for(int32 I=0; I<UE_ARRAY_COUNT(Ids); ++I) if(Id == Ids[I]) return I;
    return INDEX_NONE;
}

inline bool SupportedCount(int32 Count) { return Count == 3 || Count == 6; }
}
