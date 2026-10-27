#pragma once
#include "CoreMinimal.h"
// Value-only coordinate conversion. No CPU solver dependency.
namespace DEMCoordinates {
inline FVector3d Polar(FVector3d V) { return {V.X,-V.Y,V.Z}; }
inline FVector3d Axial(FVector3d V) { return {-V.X,V.Y,-V.Z}; }
inline FQuat Orientation(FQuat Q) { return {-Q.X,Q.Y,-Q.Z,Q.W}; }
}
