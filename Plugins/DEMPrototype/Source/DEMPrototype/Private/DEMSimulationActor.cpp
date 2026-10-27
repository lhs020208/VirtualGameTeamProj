#include "DEMSimulationActor.h"
#include "DEMParticleRenderer.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformFileManager.h"
#include "Engine/Canvas.h"
#include "DrawDebugHelpers.h"
#include "HighResScreenshot.h"
#include "UnrealClient.h"
ADEMSimulationActor::ADEMSimulationActor()
{
    PrimaryActorTick.bCanEverTick = true;
    Instances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DEMParticles"));
    SetRootComponent(Instances);
    Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Instances->SetSimulatePhysics(false);
    Instances->SetCanEverAffectNavigation(false);
    Instances->SetCastShadow(false);
}
void ADEMSimulationActor::OnConstruction(const FTransform &Transform)
{
    Super::OnConstruction(Transform);
    if (!GetWorld() || GetWorld()->IsGameWorld()) return;
    Instances->ClearInstances();
    if (!EditorPreview || !ParticleMesh || PhysicsOnly) return;
    // Preview only: no physical step, file output, or modification of other actors.
    dem::Config c;
    c.count = ParticleCount; c.seed = uint32(Seed); c.r = RadiusM; c.rho = Density;
    c.kn = NormalStiffness; c.kt = TangentStiffness;
    c.zn = c.zt = DampingRatio; c.mu = c.muWall = Friction; c.dt = FixedDt;
    const double scale = FMath::Sqrt(FMath::Max(1., ParticleCount / 200.));
    c.width *= scale; c.depth *= scale;
    Solver.reset(c);
    if (Solver.failed) return;
    Instances->SetStaticMesh(ParticleMesh);
    Instances->SetMaterial(0, ParticleMaterial);
    FDEMParticleRenderer::Update(Solver, *Instances, Transforms, true, PrepareMs, UpdateMs);
}
void ADEMSimulationActor::BeginPlay()
{
    Super::BeginPlay();
    Auto = FParse::Param(FCommandLine::Get(), TEXT("DEMAuto"));
    FParse::Value(FCommandLine::Get(), TEXT("DEMCount="), ParticleCount);
    PhysicsOnly |= FParse::Param(FCommandLine::Get(), TEXT("DEMPhysicsOnly"));
    HighResolution |= FParse::Param(FCommandLine::Get(), TEXT("DEMHigh"));
    FParse::Value(FCommandLine::Get(), TEXT("DEMTag="), RunTag);
    FParse::Value(FCommandLine::Get(), TEXT("DEMDisplayHz="), DisplayHz);
    Dense = FParse::Param(FCommandLine::Get(), TEXT("DEMDense"));
    if (Dense)
    {
        AutoBatch = 10;
        AutoEnd = 130;
    }
    ResetSimulation();
    if (Auto)
    {
        const auto initial = Solver.p;
        SingleStep();
        bool stepOK = Solver.s.steps == 1 && !Running;
        TogglePause();
        bool startOK = Running;
        TogglePause();
        bool pauseOK = !Running;
        ResetSimulation();
        bool resetOK = Solver.history.empty() && Solver.s.steps == 0;
        for (size_t i = 0; i < initial.size(); ++i)
            resetOK &= (initial[i].p - Solver.p[i].p).norm2() == 0;
        FFileHelper::SaveStringToFile(
            FString::Printf(TEXT("{\"step\":%s,\"start\":%s,\"pause\":%s,\"reset\":%s}"),
                            stepOK ? TEXT("true") : TEXT("false"), startOK ? TEXT("true") : TEXT("false"),
                            pauseOK ? TEXT("true") : TEXT("false"), resetOK ? TEXT("true") : TEXT("false")),
            *(FPaths::ProjectPluginsDir() / TEXT("DEMPrototype/Results") /
              (RunTag + TEXT("_controls.json"))));
        if (Dense)
        {
            int nx = int(Solver.c.width / (2.05 * Solver.c.r)),
                ny = int(Solver.c.depth / (2.05 * Solver.c.r));
            for (int i = 0; i < ParticleCount; ++i)
                Solver.p[i].p = {((i % nx) - .5 * (nx - 1)) * 2.05 * Solver.c.r,
                                 (((i / nx) % ny) - .5 * (ny - 1)) * 2.05 * Solver.c.r,
                                 Solver.c.r + .0001 + (i / (nx * ny)) * 2.05 * Solver.c.r};
            for (int i = 0; i < 10000 && !Solver.failed; ++i)
                Solver.step();
            if (!PhysicsOnly)
                UpdateInstances();
        }
        Running = true;
        MaxSubsteps = 100;
    }
    Snapshot = FParse::Param(FCommandLine::Get(), TEXT("DEMSnapshot"));
    if (Snapshot)
    {
        TArray<FString> lines;
        if (FFileHelper::LoadFileToStringArray(
                lines, *(FPaths::ProjectPluginsDir() / TEXT("DEMPrototype/Results/dt_snapshot.csv"))) &&
            lines.Num() == int32(Solver.p.size()))
        {
            for (int i = 0; i < lines.Num(); ++i)
            {
                TArray<FString> vals;
                lines[i].ParseIntoArray(vals, TEXT(","));
                if (vals.Num() == 7)
                {
                    auto &a = Solver.p[i];
                    a.p = {FCString::Atod(*vals[0]), FCString::Atod(*vals[1]), FCString::Atod(*vals[2])};
                    a.q = {FCString::Atod(*vals[3]), FCString::Atod(*vals[4]), FCString::Atod(*vals[5]),
                           FCString::Atod(*vals[6])};
                }
            }
            Running = false;
            Solver.statistics();
            Solver.s.time = 5;
            Solver.s.steps = uint64(5 / Solver.c.dt);
            UpdateInstances();
        }
    }
}
void ADEMSimulationActor::ResetSimulation()
{
    dem::Config c;
    c.count = ParticleCount;
    c.seed = uint32(Seed);
    c.r = RadiusM;
    c.rho = Density;
    c.kn = NormalStiffness;
    c.kt = TangentStiffness;
    c.zn = c.zt = DampingRatio;
    c.mu = c.muWall = Friction;
    c.dt = FixedDt;
    const double scale = FMath::Sqrt(FMath::Max(1., ParticleCount / 200.));
    c.width *= scale;
    c.depth *= scale;
    Solver.reset(c);
    Backlog = WallSeconds = SampleClock = DisplayClock = TextClock = 0;
    WallStart = FPlatformTime::Seconds();
    Running = false;
    Samples.Reset();
    Samples.Reserve(4096);
    Samples.Add(TEXT("time,ke,rot_ke,height,max_overlap,support,contacts,candidates,step_ms,backlog,history,"
                     "capacity,growths"));
    Transforms.Reset();
    if (Solver.failed)
    {
        RefreshText();
        return;
    }
    Transforms.Reserve(ParticleCount);
    Instances->ClearInstances();
    Instances->SetStaticMesh(
        LoadObject<UStaticMesh>(nullptr, HighResolution ? TEXT("/Engine/BasicShapes/Sphere.Sphere")
                                                        : TEXT("/DEMPrototype/SM_DEMSphere.SM_DEMSphere")));
    if (!HighResolution && ParticleMesh) Instances->SetStaticMesh(ParticleMesh);
    Instances->SetMaterial(0, ParticleMaterial);
    if (!PhysicsOnly)
        UpdateInstances(true);
    for (TActorIterator<AStaticMeshActor> it(GetWorld()); it; ++it)
    {
        if (it->GetActorLabel().StartsWith(TEXT("DEM ")))
        {
            it->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
            FVector pos = it->GetActorLocation(),
                    sz = it->GetActorScale3D() * 100; // restore baseline from wall orientation
            if (it->GetActorLabel().Contains(TEXT("floor")))
            {
                pos = FVector(0, 0, -.5);
                sz = FVector(c.width * 100 + 2, c.depth * 100 + 2, 1);
            }
            else if (it->GetActorLabel().Contains(TEXT("x")))
            {
                pos = FVector((pos.X < 0 ? -1 : 1) * (c.width * 50 + .5), 0, c.height * 50);
                sz = FVector(1, c.depth * 100 + 2, c.height * 100);
            }
            else
            {
                pos = FVector(0, (pos.Y < 0 ? -1 : 1) * (c.depth * 50 + .5), c.height * 50);
                sz = FVector(c.width * 100, 1, c.height * 100);
            }
            it->SetActorLocation(pos);
            it->SetActorScale3D(sz / 100);
        }
    }
    RefreshText();
}
void ADEMSimulationActor::TogglePause()
{
    Running = !Running && !Solver.failed;
}
void ADEMSimulationActor::SingleStep()
{
    Running = false;
    if (Solver.step())
    {
        LastSubsteps = 1;
        if (!PhysicsOnly)
            UpdateInstances();
    }
    else
        SaveResults();
    RefreshText();
}
void ADEMSimulationActor::UpdateInstances(bool Rebuild)
{
    // A HUD Step may arrive after Tick; defer a second upload to the next display interval.
    if (!Rebuild && LastRenderFrame == GFrameCounter)
        return;
    LastRenderFrame = GFrameCounter;
    FDEMParticleRenderer::Update(Solver, *Instances, Transforms, Rebuild, PrepareMs, UpdateMs);
}
void ADEMSimulationActor::Tick(float dt)
{
    Super::Tick(dt);
    WallSeconds = FPlatformTime::Seconds() - WallStart;
    DisplayClock += dt;
    TextClock += dt;
    LastSubsteps = 0;
    PrepareMs = UpdateMs = 0;
    if (APlayerController *pc = GetWorld()->GetFirstPlayerController())
    {
        if (pc->WasInputKeyJustPressed(EKeys::SpaceBar))
            TogglePause();
        if (pc->WasInputKeyJustPressed(EKeys::N))
        {
            SingleStep();
            DisplayClock = 0;
        }
        if (pc->WasInputKeyJustPressed(EKeys::R))
            ResetSimulation();
        if (pc->WasInputKeyJustPressed(EKeys::S))
            SaveResults();
    }
    if (Running)
    {
        if (Auto)
        {
            for (int i = 0; i < AutoBatch && !Solver.failed; ++i)
            {
                Solver.step();
                ++LastSubsteps;
            }
        }
        else
        {
            Backlog += dt * FMath::Clamp(Speed, 0., 10.);
            if (Backlog > 2)
            {
                Running = false;
            }
            else
                while (Backlog >= Solver.c.dt && LastSubsteps < MaxSubsteps && !Solver.failed)
                {
                    Solver.step();
                    Backlog -= Solver.c.dt;
                    ++LastSubsteps;
                }
        }
    }
    if (Solver.failed)
    {
        Running = false;
        SaveResults();
        SetActorTickEnabled(false);
    }
    if (!PhysicsOnly && DisplayClock >= 1 / FMath::Clamp(DisplayHz, 1., 240.))
    {
        UpdateInstances();
        DisplayClock = 0;
    }
    if (ShowRotation)
        for (int i = 0; i < FMath::Min(10, Transforms.Num()); ++i)
        {
            auto &t = Transforms[i];
            DrawDebugLine(GetWorld(), t.GetLocation(),
                          t.GetLocation() + t.GetRotation().RotateVector(FVector(0, 0, 3)), FColor::Red,
                          false, 0, 0, .15f);
        }
    if (TextClock >= .1)
    {
        RefreshText();
        TextClock = 0;
    }
    if (Solver.s.time - SampleClock >= .02)
    {
        const auto &s = Solver.s;
        Samples.Add(FString::Printf(
            TEXT("%.9g,%.12g,%.12g,%.9g,%.9g,%.9g,%llu,%llu,%.9g,%.9g,%llu,%llu,%llu"), s.time, s.ke, s.rotKE,
            s.height, s.maxOverlap, s.support, s.contacts, s.candidates, s.stepMs, Backlog,
            uint64(Solver.history.size()), uint64(s.historyCapacity), s.growths));
        SampleClock = s.time;
        if (Samples.Num() >= 4096)
            SaveResults();
    }
    if (Auto)
    {
        ++AutoFrames;
        AutoSteps += LastSubsteps;
        if (AutoFrames > 30)
        {
            FrameTimes.Add(dt * 1000.);
            PrepTimes.Add(PhysicsOnly ? 0 : PrepareMs);
            UpdateTimes.Add(PhysicsOnly ? 0 : UpdateMs);
            StepTimes.Add(Solver.s.stepMs);
        }
        if (!Dense && (AutoFrames == 40 || AutoFrames == AutoEnd - 20))
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectPluginsDir() / TEXT("DEMPrototype/Results") /
                                                      (RunTag + FString::Printf(TEXT("_%d.png"), AutoFrames)),
                                                  false, false);
        }
        if (AutoFrames == AutoEnd || (Snapshot && AutoFrames == 45))
        {
            SaveResults();
            FString report = TEXT("mode,particles,steps,sim_time,mean_frame_ms,p95_frame_ms,mean_prepare_ms,"
                                  "mean_update_ms,mean_step_ms,ke,rot_ke,height,history\n");
            auto mean = [](const TArray<double> &a)
            {
                double v = 0;
                for (double x : a)
                    v += x;
                return a.Num() ? v / a.Num() : 0.;
            };
            FrameTimes.Sort();
            report +=
                FString::Printf(TEXT("%s,%d,%d,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g,%.17g,%.17g,%.17g,%llu\n"),
                                *RunTag, ParticleCount, AutoSteps, Solver.s.time, mean(FrameTimes),
                                FrameTimes[FMath::FloorToInt(.95 * (FrameTimes.Num() - 1))], mean(PrepTimes),
                                mean(UpdateTimes), mean(StepTimes), Solver.s.ke, Solver.s.rotKE,
                                Solver.s.height, uint64(Solver.history.size()));
            FFileHelper::SaveStringToFile(report,
                                          *(FPaths::ProjectPluginsDir() / TEXT("DEMPrototype/Results") /
                                            (RunTag + TEXT("_render.csv"))));
            FString states = TEXT("id,x,y,z,vx,vy,vz,wx,wy,wz,qx,qy,qz,qw,fx,fy,fz,tx,ty,tz\n");
            for (const auto &a : Solver.p)
                states +=
                    FString::Printf(TEXT("%u,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%."
                                         "17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g\n"),
                                    a.id, a.p.x, a.p.y, a.p.z, a.v.x, a.v.y, a.v.z, a.w.x, a.w.y, a.w.z,
                                    a.q.x, a.q.y, a.q.z, a.q.w, a.f.x, a.f.y, a.f.z, a.t.x, a.t.y, a.t.z);
            FFileHelper::SaveStringToFile(states,
                                          *(FPaths::ProjectPluginsDir() / TEXT("DEMPrototype/Results") /
                                            (RunTag + TEXT("_state.csv"))));
            FString hist = TEXT("key,xi_x,xi_y,xi_z,n_x,n_y,n_z\n");
            for (const auto &h : Solver.history)
                hist += FString::Printf(TEXT("%llu,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g\n"), h.key, h.xi.x,
                                        h.xi.y, h.xi.z, h.n.x, h.n.y, h.n.z);
            FFileHelper::SaveStringToFile(hist, *(FPaths::ProjectPluginsDir() / TEXT("DEMPrototype/Results") /
                                                  (RunTag + TEXT("_history.csv"))));
            FPlatformMisc::RequestExit(false);
        }
    }
}
void ADEMSimulationActor::SaveResults()
{
    const FString path = FPaths::ProjectSavedDir() / TEXT("DEMPrototype");
    IFileManager::Get().MakeDirectory(*path, true);
    FString name = RunTag.IsEmpty() ? TEXT("interactive") : RunTag;
    FFileHelper::SaveStringArrayToFile(Samples, *(path / (name + TEXT(".csv"))),
                                       FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
                                       &IFileManager::Get(), FILEWRITE_Append);
    Samples.Reset();
    if (Solver.failed)
        FFileHelper::SaveStringToFile(UTF8_TO_TCHAR(Solver.error), *(path / (name + TEXT("_error.txt"))));
}
void ADEMSimulationActor::RefreshText()
{
    const auto &s = Solver.s;
    DisplayText = FString::Printf(
        TEXT("CPU DEM | %s | SI double | 1 m = 100 uu\nPhysics %d / visible %d | mass %.6f kg\nt %.4f s | "
             "wall %.2f s | ratio %.3f | dt %.6g\ncontacts %llu | candidates %llu | max v %.3f m/s\nKE %.5g "
             "J | rotation %.5g J | overlap %.3f %%\nBottom %.4f N / weight %.4f N | height %.3f m\nEscapes "
             "%llu | floor %llu | errors %llu\nDEM %.3f ms (grid %.3f search %.3f contact %.3f)\nPrepare "
             "%.3f / ISM %.3f ms | substeps %d | lag %.3f s\nSpeed %.2fx | display %.0f Hz | history %llu "
             "peak %llu\nSpace start/pause | N step | R reset | S save\nReset settings: actor Details "
             "(applied on Reset)\nLag > 2 s pauses, preserves pending time. Reset clears lag."),
        Running ? TEXT("RUNNING") : TEXT("PAUSED"), int32(Solver.p.size()), Instances->GetInstanceCount(),
        s.mass, s.time, WallSeconds, WallSeconds > 0 ? s.time / WallSeconds : 0, Solver.c.dt, s.contacts,
        s.candidates, s.maxSpeed, s.ke, s.rotKE, s.maxOverlap * 100, s.support, s.mass * 9.81, s.height,
        s.escapes, s.floorCrossings, s.errors, s.stepMs, s.gridMs, s.searchMs, s.contactMs, PrepareMs,
        UpdateMs, LastSubsteps, Backlog, Speed, DisplayHz, uint64(Solver.history.size()),
        uint64(s.historyPeak));
    if (Snapshot)
        DisplayText = TEXT("Verified 5 s deposition: geometry snapshot\nForces/velocities are not replayed "
                           "in this view.\n") +
                      DisplayText;
}
void ADEMHUD::DrawHUD()
{
    Super::DrawHUD();
    ADEMSimulationActor *a = nullptr;
    for (TActorIterator<ADEMSimulationActor> it(GetWorld()); it; ++it)
    {
        a = *it;
        break;
    }
    if (!a)
        return;
    DrawRect(FLinearColor(0, 0, 0, .8f), 10, 10, 565, 365);
    DrawText(a->DisplayText, FLinearColor::White, 20, 20, nullptr, 1.1f);
    const TCHAR *labels[] = {TEXT("Start/Pause"), TEXT("Step"),     TEXT("Reset"), TEXT("Speed -"),
                             TEXT("Speed +"),     TEXT("Hz 10/60"), TEXT("Save")};
    for (int i = 0; i < 7; ++i)
    {
        float x = 20 + i * 78;
        DrawRect(FLinearColor(.1f, .25f, .35f, 1), x, 345, 74, 25);
        DrawText(labels[i], FLinearColor::White, x + 2, 350, nullptr, .85f);
        AddHitBox(FVector2D(x, 345), FVector2D(74, 25), FName(labels[i]), true);
    }
}
void ADEMHUD::NotifyHitBoxClick(FName name)
{
    for (TActorIterator<ADEMSimulationActor> it(GetWorld()); it; ++it)
    {
        auto *a = *it;
        if (name == TEXT("Start/Pause"))
            a->TogglePause();
        else if (name == TEXT("Step"))
            a->SingleStep();
        else if (name == TEXT("Reset"))
            a->ResetSimulation();
        else if (name == TEXT("Speed -"))
            a->Speed = FMath::Max(.01, a->Speed * .5);
        else if (name == TEXT("Speed +"))
            a->Speed = FMath::Min(10., a->Speed * 2);
        else if (name == TEXT("Hz 10/60"))
            a->DisplayHz = a->DisplayHz > 10 ? 10 : 60;
        else if (name == TEXT("Save"))
            a->SaveResults();
        break;
    }
}
ADEMGameMode::ADEMGameMode()
{
    HUDClass = ADEMHUD::StaticClass();
    DefaultPawnClass = nullptr;
}
void ADEMGameMode::BeginPlay()
{
    Super::BeginPlay();
    if (auto *pc = GetWorld()->GetFirstPlayerController())
    {
        pc->bShowMouseCursor = true;
        pc->bEnableClickEvents = true;
        for (TActorIterator<ACameraActor> it(GetWorld()); it; ++it)
        {
            double scale = 1;
            for (TActorIterator<ADEMSimulationActor> a(GetWorld()); a; ++a)
            {
                if (a->UseLevelCamera) { pc->SetViewTarget(*it); return; }
                scale = FMath::Sqrt(FMath::Max(1., a->ParticleCount / 200.));
                break;
            }
            FVector position = FVector(25, -35, 110) * scale, target = FVector(12, 10, 10) * scale;
            it->SetActorLocation(position);
            it->SetActorRotation((target - position).Rotation());
            it->GetCameraComponent()->FieldOfView = 45;
            pc->SetViewTarget(*it);
            break;
        }
    }
}



