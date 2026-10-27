#pragma once
#include "CoreMinimal.h"
#include "DEMSolver.h"
// Single reflection boundary: polar H=(+,-,+), axial det(H)H=(-,+,-).
namespace DEMCoordinates
{
inline FVector3d Polar(FVector3d V) { return {V.X,-V.Y,V.Z}; }
inline FVector3d Axial(FVector3d V) { return {-V.X,V.Y,-V.Z}; }
inline dem::V Vector(FVector3d V) { return {V.X,V.Y,V.Z}; }
inline FVector3d Vector(dem::V V) { return {V.x,V.y,V.z}; }
inline dem::V PositionToDEM(FVector V) { return Vector(Polar(V*.01)); }
inline FVector PositionToUE(dem::V V) { return Polar(Vector(V))*100.; }
inline dem::V PolarToDEM(FVector V) { return Vector(Polar(V)); }
inline dem::V AxialToDEM(FVector V) { return Vector(Axial(V)); }
inline FQuat Orientation(FQuat Q) { return {-Q.X,Q.Y,-Q.Z,Q.W}; }
inline dem::Q Quaternion(FQuat Q) { Q=Orientation(Q);return {Q.X,Q.Y,Q.Z,Q.W}; }
inline FQuat Quaternion(dem::Q Q) { return {-Q.x,Q.y,-Q.z,Q.w}; }
}
