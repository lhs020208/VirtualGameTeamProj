#include "DEMSandboxCommandlet.h"
#include "DEMSimulationActor.h"
#include "Modules/ModuleManager.h"
#include "Editor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "HAL/FileManager.h"
#include "FileHelpers.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "StaticMeshAttributes.h"
#include "MeshDescription.h"
#include "UObject/SavePackage.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "StaticMeshResources.h"
IMPLEMENT_MODULE(FDefaultModuleImpl, DEMPrototypeEditor)
UDEMSandboxCommandlet::UDEMSandboxCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}
int32 UDEMSandboxCommandlet::Main(const FString &Params)
{
    if (Params.Contains(TEXT("DEMVerifyProjectContent")))
    {
        auto *bp = LoadObject<UBlueprint>(nullptr, TEXT("/Game/DEM/BP_DEMSimulation.BP_DEMSimulation"));
        auto *world = LoadObject<UWorld>(nullptr, TEXT("/Game/DEM/DEM_Demo.DEM_Demo"));
        if (!bp || !bp->GeneratedClass || !world || !world->PersistentLevel) return 20;
        auto *defaults = Cast<ADEMSimulationActor>(bp->GeneratedClass->GetDefaultObject());
        if (!defaults || !defaults->ParticleMesh || !defaults->EditorPreview || !defaults->UseLevelCamera) return 21;
        if (defaults->ParticleMesh->GetPathName() != TEXT("/Game/DEM/SM_DEMSphere.SM_DEMSphere")) return 22;
        int count = 0;
        for (AActor *actor : world->PersistentLevel->Actors)
            if (auto *simulation = Cast<ADEMSimulationActor>(actor))
            {
                ++count;
                if (simulation->GetClass() != bp->GeneratedClass || simulation->ParticleCount != 200) return 23;
            }
        if (count != 1) return 24;
        UE_LOG(LogTemp, Display, TEXT("DEM project content verified: blueprint, mesh, preview defaults, level camera, map actor"));
        return 0;
    }
    const bool ProjectContent = Params.Contains(TEXT("DEMProjectContent"));
    const FString root = ProjectContent ? TEXT("/Game/DEM") : TEXT("/DEMPrototype");
    const FString mapName = ProjectContent ? TEXT("DEM_Demo") : TEXT("DEM_Sandbox");
    const FString out = ProjectContent ? FPaths::ProjectContentDir() / TEXT("DEM")
                                      : FPaths::ProjectPluginsDir() / TEXT("DEMPrototype/Content");
    IFileManager::Get().MakeDirectory(*out, true);
    const bool MeshOnly = Params.Contains(TEXT("DEMMeshOnly"));
    if (!MeshOnly && (FPaths::FileExists(out / (mapName + TEXT(".umap"))) ||
        FPaths::FileExists(out / TEXT("SM_DEMSphere.uasset"))))
    {
        UE_LOG(LogTemp, Error, TEXT("Refusing to overwrite existing DEM assets"));
        return 2;
    }
    const FString packageName = root / TEXT("SM_DEMSphere");
    UPackage *pkg = MeshOnly ? LoadPackage(nullptr, *packageName, LOAD_None) : CreatePackage(*packageName);
    if (!pkg) return 6;
    pkg->FullyLoad();
    UStaticMesh *mesh = FindObject<UStaticMesh>(pkg, TEXT("SM_DEMSphere"));
    if (!mesh) mesh = NewObject<UStaticMesh>(pkg, TEXT("SM_DEMSphere"), RF_Public | RF_Standalone);
    FMeshDescription md;
    FStaticMeshAttributes attr(md);
    attr.Register();
    auto pos = attr.GetVertexPositions();
    auto normals = attr.GetVertexInstanceNormals();
    auto uvs = attr.GetVertexInstanceUVs();
    uvs.SetNumChannels(1);
    auto group = md.CreatePolygonGroup();
    const int lon = 12, lat = 8;
    TArray<FVertexID> verts;
    for (int j = 0; j <= lat; ++j)
        for (int i = 0; i < lon; ++i)
        {
            double theta = PI * j / lat, phi = 2 * PI * i / lon;
            auto id = md.CreateVertex();
            pos[id] = FVector3f(FMath::Sin(theta) * FMath::Cos(phi), FMath::Sin(theta) * FMath::Sin(phi),
                                FMath::Cos(theta));
            verts.Add(id);
        }
    auto triangle = [&](int a, int b, int c)
    {
        TArray<FVertexInstanceID> vi;
        for (int index : {a, b, c})
        {
            auto v = md.CreateVertexInstance(verts[index]);
            normals[v] = pos[verts[index]].GetSafeNormal();
            uvs.Set(v, 0, FVector2f(float(index % lon) / lon, float(index / lon) / lat));
            vi.Add(v);
        }
        md.CreatePolygon(group, vi);
    };
    for (int j = 0; j < lat; ++j)
        for (int i = 0; i < lon; ++i)
        {
            int a = j * lon + i, b = j * lon + (i + 1) % lon, c = (j + 1) * lon + i,
                d = (j + 1) * lon + (i + 1) % lon;
            if (j > 0)
                triangle(a, b, c);
            if (j < lat - 1)
                triangle(b, d, c);
        }
    mesh->GetStaticMaterials().Reset();
    mesh->GetStaticMaterials().Add(FStaticMaterial(LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"))));
    mesh->SetNumSourceModels(1);
    mesh->GetSourceModel(0).BuildSettings.bRecomputeNormals = false;
    mesh->GetSourceModel(0).BuildSettings.bRecomputeTangents = true;
    mesh->GetSourceModel(0).BuildSettings.bGenerateLightmapUVs = false;
    mesh->CreateMeshDescription(0, MoveTemp(md));
    mesh->CommitMeshDescription(0);
    mesh->Build(false);
    mesh->MarkPackageDirty();
    FSavePackageArgs args;
    args.TopLevelFlags = RF_Public | RF_Standalone;
    if (!UPackage::SavePackage(pkg, mesh, *(out / TEXT("SM_DEMSphere.uasset")), args))
        return 3;
    FString info = TEXT("mesh,vertices,triangles,radius_uu\n");
    for (UStaticMesh *m : {mesh, LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"))})
        if (m && m->GetRenderData() && m->GetRenderData()->LODResources.Num())
        {
            auto &lod = m->GetRenderData()->LODResources[0];
            info += FString::Printf(TEXT("%s,%u,%u,%.9g\n"), *m->GetName(), lod.GetNumVertices(),
                                    lod.GetNumTriangles(), m->GetBounds().BoxExtent.X);
        }
    FFileHelper::SaveStringToFile(info,
                                  *(FPaths::ProjectPluginsDir() / TEXT("DEMPrototype/Results/meshes.csv")));
    if (MeshOnly)
        return 0;
    UWorld *world = GEditor->NewMap();
    if (!world)
        return 4;
    world->GetWorldSettings()->DefaultGameMode = ADEMGameMode::StaticClass();
    UClass *simulationClass = ADEMSimulationActor::StaticClass();
    if (ProjectContent)
    {
        const FString bpName = root / TEXT("BP_DEMSimulation");
        if (FPaths::FileExists(out / TEXT("BP_DEMSimulation.uasset"))) return 7;
        UPackage *bpPackage = CreatePackage(*bpName);
        UBlueprint *bp = FKismetEditorUtilities::CreateBlueprint(ADEMSimulationActor::StaticClass(),
            bpPackage, TEXT("BP_DEMSimulation"), BPTYPE_Normal, UBlueprint::StaticClass(),
            UBlueprintGeneratedClass::StaticClass(), TEXT("DEMProjectContent"));
        if (!bp) return 8;
        FKismetEditorUtilities::CompileBlueprint(bp);
        auto *defaults = Cast<ADEMSimulationActor>(bp->GeneratedClass->GetDefaultObject());
        if (!defaults) return 9;
        defaults->ParticleMesh = mesh;
        defaults->EditorPreview = true;
        defaults->UseLevelCamera = true;
        bp->MarkPackageDirty();
        if (!UPackage::SavePackage(bpPackage, bp, *(out / TEXT("BP_DEMSimulation.uasset")), args)) return 10;
        simulationClass = bp->GeneratedClass;
    }
    auto *simulation = world->SpawnActor<ADEMSimulationActor>(simulationClass);
    simulation->SetActorLabel(TEXT("DEM Simulation - edit settings here"));
    UStaticMesh *cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto box = [&](FVector center, FVector size, const TCHAR *label)
    {
        auto *a = world->SpawnActor<AStaticMeshActor>(center, FRotator::ZeroRotator);
        a->SetActorLabel(label);
        a->GetStaticMeshComponent()->SetStaticMesh(cube);
        a->SetActorScale3D(size / 100);
        a->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    };
    box(FVector(0, 0, -.5), FVector(32, 32, 1), TEXT("DEM floor z=0"));
    box(FVector(-15.5, 0, 12.5), FVector(1, 32, 25), TEXT("DEM x-"));
    box(FVector(15.5, 0, 12.5), FVector(1, 32, 25), TEXT("DEM x+"));
    box(FVector(0, -15.5, 12.5), FVector(30, 1, 25), TEXT("DEM y+"));
    box(FVector(0, 15.5, 12.5), FVector(30, 1, 25), TEXT("DEM y-"));
    auto *light = world->SpawnActor<ADirectionalLight>(FVector(0, 0, 100), FRotator(-55, -35, 0));
    light->GetLightComponent()->SetIntensity(8);
    auto *sky = world->SpawnActor<ASkyLight>();
    sky->GetLightComponent()->SetIntensity(1);
    FVector camera(55, -65, 85), target(0, 0, 10);
    auto *cam = world->SpawnActor<ACameraActor>(camera, (target - camera).Rotation());
    cam->GetCameraComponent()->FieldOfView = 45;
    if (!UEditorLoadingAndSavingUtils::SaveMap(world, root / mapName))
        return 5;
    return 0;
}




