#pragma once
#include "CoreMinimal.h"
#include "DEMSolver.h"
#include "DEMCoordinates.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
// Pure display boundary: never modifies solver state. Index i retains particle i's ID.
struct FDEMParticleRenderer
{
    static void Update(const dem::Solver &Solver, UInstancedStaticMeshComponent &ISM,
                       TArray<FTransform> &Buffer, bool Rebuild, double &PrepareMs, double &UploadMs)
    {
        const double start = FPlatformTime::Seconds();
        Buffer.Reset();
        const UStaticMesh *mesh = ISM.GetStaticMesh();
        if (!mesh)
            return;
        const double sourceRadius = mesh->GetBounds().BoxExtent.X;
        if (sourceRadius <= 0)
            return;
        for (const auto &a : Solver.p)
            Buffer.Emplace(DEMCoordinates::Quaternion(a.q),
                           DEMCoordinates::PositionToUE(a.p),
                           FVector(a.r * 100 / sourceRadius));
        PrepareMs = (FPlatformTime::Seconds() - start) * 1000;
        const double upload = FPlatformTime::Seconds();
        if (Rebuild)
            ISM.AddInstances(Buffer, false, false, false);
        else
            ISM.BatchUpdateInstancesTransforms(0, Buffer, false, true, true);
        UploadMs = (FPlatformTime::Seconds() - upload) * 1000;
    }
};
