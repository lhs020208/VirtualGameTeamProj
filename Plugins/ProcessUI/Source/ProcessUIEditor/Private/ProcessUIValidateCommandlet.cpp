#include "ProcessUIValidateCommandlet.h"
#include "ProcessUIRig.h"
#include "ProcessController.h"
#include "ProcessTestProvider.h"
#include "FileHelpers.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/LightComponent.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
using namespace ProcessUI;
namespace
{
class FDelayedSnapshotProvider : public IProvider
{
public:
    TSharedRef<FTestProvider> Inner;
    FSnapshot View;
    bool Freeze=false,StaleConditions=false;
    explicit FDelayedSnapshotProvider(const TArray<FHopper>& H):Inner(MakeShared<FTestProvider>(H)),View(Inner->Snapshot()){}
    void Tick(double D)override{Inner->Tick(D);if(!Freeze){View=Inner->Snapshot();if(StaleConditions)++View.ConditionsRequest;}}
    const FSnapshot& Snapshot()const override{return View;}
    FBounds Bounds()const override{return Inner->Bounds();}
    void Submit(const FCommand& C)override{Inner->Submit(C);if(!Freeze)View=Inner->Snapshot();}
};
}
UProcessUIValidateCommandlet::UProcessUIValidateCommandlet() { IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true; }
int32 UProcessUIValidateCommandlet::Main(const FString& Params)
{
    int32 Failures=0;FString Report;
    auto Check=[&](bool Ok,const FString& Name){Report+=(Ok?TEXT("PASS "):TEXT("FAIL "))+Name+TEXT("\n");if(!Ok)++Failures;UE_LOG(LogTemp,Display,TEXT("ProcessUI %s: %s"),Ok?TEXT("PASS"):TEXT("FAIL"),*Name);};
    auto Advance=[](FController& C,int32 Count){for(int32 I=0;I<Count;++I)C.Tick(.05);};
    {
        auto P=MakeShared<FTestProvider>(DemoRecipe().Hoppers); FController C(P);C.Recipe=DemoRecipe();
        Check(C.Validate(C.Recipe).IsEmpty(),TEXT("example validation")); Check(C.Start(),TEXT("initial start"));
        bool SawScheduled=false,KeptMixing=true;double MixingStartedAt=-1;
        for(int32 I=0;I<1000;++I)
        {
            C.Tick(.05);
            if(MixingStartedAt<0&&C.Snapshot().ActualRPM.Valid()&&C.Snapshot().ActualRPM.Value>0)MixingStartedAt=C.Snapshot().SourceSeconds;
            if(C.StepIndex==4&&C.Operation==EOperation::Charge)
            {if(!SawScheduled)Check(FMath::IsNearlyEqual(C.MixingSeconds,12.0,.051),TEXT("scheduled time is mixer charge start"));SawScheduled=true;KeptMixing&=C.Snapshot().ActualRPM.Valid()&&C.Snapshot().ActualRPM.Value>0;}
        }
        Check(SawScheduled&&KeptMixing,TEXT("scheduled addition preserves mixing"));
        Check(FMath::IsNearlyEqual(C.Snapshot().ChargeStartedAt.FindRef(C.Recipe.Steps[4].Id)-MixingStartedAt,12.0,1e-7),TEXT("provider confirms charge start at reservation, not delayed request"));
        Check(C.Run==ERun::Complete,TEXT("full initial and scheduled sequence completes"));
        const FDevice* M=C.Snapshot().Devices.FindByPredicate([](const FDevice& D){return D.Id==TEXT("mixer");});
        Check(M&&M->Mass.Valid()&&FMath::IsNearlyEqual(M->Mass.Value,.125,1e-7),TEXT("all material follows common route; mass conserved"));
        auto Zero=C.Recipe;Zero.Steps[4].AtSeconds=0;
        Check(!C.Validate(Zero).IsEmpty(),TEXT("scheduled zero distinct from initial; impossible receiver schedule rejected"));
    }
    for(int32 Mode=0;Mode<3;++Mode)
    {
        auto P=MakeShared<FTestProvider>(DemoRecipe().Hoppers);FController C(P);C.Recipe=DemoRecipe();C.Start();bool Reached=false;
        for(int32 I=0;I<1000;++I)
        {
            C.Tick(.05);
            if((Mode==0&&C.Operation==EOperation::Transfer&&C.Snapshot().Transferred.FindRef(C.Recipe.Steps[0].Id)>0)||
               (Mode==1&&C.StepIndex==4&&C.Operation==EOperation::Charge&&C.Snapshot().Charged.FindRef(C.Recipe.Steps[4].Id)>0)||
               (Mode==2&&C.StepIndex==3&&C.Snapshot().ActualRPM.Value==0)) {Reached=true;break;}
        }
        const FString Name=Mode==0?TEXT("partial receiver transfer"):Mode==1?TEXT("partial scheduled mixer charge"):TEXT("initial mixing command pending");
        Check(Reached,Name+TEXT(" reached"));C.Pause(TEXT("boundary test"));const double Before=C.MixingSeconds;Advance(C,20);
        Check(C.MixingSeconds==Before&&C.Resume(),Name+TEXT(" explicit resume with frozen clock"));Advance(C,1000);
        double Charged=0;for(const auto& X:C.Snapshot().Charged)Charged+=X.Value;
        Check(C.Run==ERun::Complete&&FMath::IsNearlyEqual(Charged,.125,1e-7),Name+TEXT(" completes without duplicate material"));
    }
    {
        auto P=MakeShared<FDelayedSnapshotProvider>(DemoRecipe().Hoppers);FController C(P);C.Recipe=DemoRecipe();C.Start();P->StaleConditions=true;Advance(C,150);
        Check(C.StepIndex==0,TEXT("old command completion flags cannot advance sequence"));P->StaleConditions=false;C.Tick(.05);C.Pause(TEXT("connection test"));Advance(C,10);P->Freeze=true;Advance(C,50);
        Check(!C.ConfirmedSafe()&&!C.Resume(),TEXT("stale stopped snapshot cannot authorize resume"));
    }
    {
        auto R=DemoRecipe();R.Hoppers.Add({TEXT("supply-4"),TEXT("가상 D"),false,.0001,.025});FStep Extra;Extra.HopperId=TEXT("supply-4");Extra.TargetKg=.02;Extra.Name=TEXT("추가 호퍼 칭량");R.Steps.Insert(Extra,2);
        auto P=MakeShared<FTestProvider>(R.Hoppers);FController C(P);C.Recipe=R;Check(C.Start(),TEXT("fourth supply hopper accepted dynamically"));Advance(C,1100);Check(C.Run==ERun::Complete,TEXT("four-hopper sequence completes"));
    }
    {
        auto P=MakeShared<FTestProvider>(DemoRecipe().Hoppers);FController C(P);C.Recipe=DemoRecipe();C.Start();
        Advance(C,12); C.ManualValve(TEXT("supply-1.outlet"),false);
        Check(C.Run==ERun::Paused,TEXT("manual intervention pauses sequence"));
        double Time=C.MixingSeconds;Advance(C,20);Check(C.ConfirmedSafe()&&C.MixingSeconds==Time,TEXT("stop acknowledgement separate; clock frozen"));
        const double Before=C.Snapshot().Weighed.FindRef(C.Recipe.Steps[0].Id);
        Check(Before>0&&Before<.05,TEXT("partial amount retained"));Check(C.Resume(),TEXT("explicit resume"));Advance(C,1000);
        Check(C.Run==ERun::Complete&&FMath::IsNearlyEqual(C.Snapshot().Weighed.FindRef(C.Recipe.Steps[0].Id),.05,1e-8),TEXT("resume only remaining amount, no duplicate dose"));
    }
    {
        auto P=MakeShared<FTestProvider>(DemoRecipe().Hoppers);FController C(P);C.Recipe=DemoRecipe();C.Start();
        for(int32 I=0;I<1000&&C.StepIndex<3;++I)C.Tick(.05);
        P->PreparationSpeed=.001;Advance(C,300);
        Check(C.Run==ERun::Paused&&FMath::IsNearlyEqual(C.MixingSeconds,12.0,1e-7),TEXT("unready deadline stops at scheduled clock"));
        Check(C.Snapshot().Charged.FindRef(C.Recipe.Steps[4].Id)==0,TEXT("no automatic late charge"));
        Check(C.MustReset&&!C.Resume(),TEXT("missed reservation requires abort and reset; resume forbidden"));
        P->PreparationSpeed=1;Advance(C,100);
        Check(!C.Resume()&&C.Snapshot().Charged.FindRef(C.Recipe.Steps[4].Id)==0&&FMath::IsNearlyEqual(C.MixingSeconds,12.0,1e-7),TEXT("recovered preparation cannot resume missed additive dose"));
        Check(!C.Reset(),TEXT("missed reservation requires explicit batch abort before reset"));
        C.Abort();Check(!C.Reset(),TEXT("missed reservation reset waits for latest stop confirmation"));Advance(C,10);
        Check(C.Reset(),TEXT("missed reservation reset allowed after abort and confirmed stop"));Advance(C,2);
        Check(C.Run==ERun::Idle&&!C.MustReset&&C.MixingSeconds==0&&C.Snapshot().Charged.IsEmpty(),TEXT("missed reservation reset clears batch without automatic restart"));
    }
    {
        auto P=MakeShared<FTestProvider>(DemoRecipe().Hoppers);FController C(P);C.Recipe=DemoRecipe();C.Start();Advance(C,5);
        P->FailCloseValve=TEXT("receiver.outlet");C.Pause(TEXT("test"));Advance(C,10);
        Check(!C.ConfirmedSafe()&&!C.Resume(),TEXT("failed close blocks resume"));
        P->FailCloseValve.Empty();C.ManualValve(TEXT("receiver.outlet"),false);Check(C.ConfirmedSafe(),TEXT("independent close works before full stop confirmation"));
        P->InjectWrongMaterial();Check(!C.Resume(),TEXT("wrong material prevents normal resume"));C.Abort();Check(!C.Reset(),TEXT("reset waits for current stop request confirmation"));Advance(C,10);Check(C.Reset(),TEXT("reset requested after abort and safe confirmation"));Advance(C,2);Check(C.Run==ERun::Idle,TEXT("reset generation confirmed without automatic restart"));
    }
    {
        FString E;const FString Dir=FPaths::ProjectSavedDir()/TEXT("ProcessUI/ValidationRecipes");auto R=DemoRecipe();R.Name=TEXT("roundtrip");
        Check(SaveRecipe(R,Dir,E),TEXT("named save"));FRecipe Loaded;Check(LoadRecipe(R.Name,Dir,Loaded,E),TEXT("named load"));
        auto P=MakeShared<FTestProvider>(Loaded.Hoppers);FController C(P);C.Recipe=Loaded;
        Check(C.Run==ERun::Idle&&C.Validate(Loaded).IsEmpty()&&Loaded.Steps.Num()==5&&Loaded.Hoppers[2].Additive,TEXT("roundtrip preserved; validates without execution"));
        FFileHelper::SaveStringToFile(TEXT("{\"version\":999}"),*(Dir/TEXT("invalid.json")));
        Check(!LoadRecipe(TEXT("invalid"),Dir,Loaded,E),TEXT("invalid schema rejected"));
        Check(!SaveRecipe(FRecipe{1,TEXT("../escape")},Dir,E),TEXT("path traversal rejected"));
        Check(FMath::IsNearlyEqual(.1/1000,100.0/1000000,1e-12),TEXT("diameter mm to micrometers"));
        Check(Display(FReading{})==TEXT("미지원"),TEXT("unsupported is not zero"));
    }
    const FString Map=TEXT("/Game/ProcessUI/Maps/L_ProcessUI_Test");
    const FString Filename=FPaths::ProjectContentDir()/TEXT("ProcessUI/Maps/L_ProcessUI_Test.umap");
    if(!IFileManager::Get().FileExists(*Filename))
    {
        UWorld* W=UEditorLoadingAndSavingUtils::NewBlankMap(false);
        Check(W!=nullptr,TEXT("new isolated test world"));
        if(W)
        {
            auto* Rig=W->SpawnActor<AProcessUIRig>();Rig->SetActorLabel(TEXT("ProcessUI_TestRig"));
            auto* Sun=W->SpawnActor<ADirectionalLight>();Sun->SetActorRotation(FRotator(-50,-30,0));Sun->GetLightComponent()->SetIntensity(5);
            W->SpawnActor<ASkyLight>();
            Check(UEditorLoadingAndSavingUtils::SaveMap(W,Map),TEXT("save isolated test level"));
        }
    }
    UWorld* LoadedWorld=UEditorLoadingAndSavingUtils::LoadMap(Filename);
    bool Found=false;if(LoadedWorld)for(TActorIterator<AProcessUIRig> It(LoadedWorld);It;++It)Found=true;
    Check(LoadedWorld&&Found,TEXT("load test level with UI rig"));
    IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("ProcessUI")),true);
    FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("ProcessUI/Validation.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    return Failures?1:0;
}
