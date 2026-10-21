#pragma once
#include "CoreMinimal.h"
#include "powder/mesh.hpp"
class UStaticMeshComponent;
// Read rendered LOD0, bake its complete component transform into solver-local SI coordinates.
// Editor build only; closed outward manifold analysis meshes. No guessed scale or automatic hole closing.
powder::StaticTriangleMesh ExtractDEMStaticMesh(const UStaticMeshComponent& Component,FVector SolverOriginCm,double MinDiagonalM,double MaxDiagonalM);
