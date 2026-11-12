#include "ProcessUIStateAdapter.h"
#include "ProcessControlService.h"
#include "ProcessControlUI.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace
{
// Explicitly invoked, dedicated test process only. Never runs during ordinary PIE/play.
FAutoConsoleCommandWithWorld ManualScreenTest(TEXT("ProcessUI.ValidateManualScreen"),
    TEXT("Development test: invokes actual UMG button delegates, captures the game window, then exits."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        const TWeakObjectPtr<UWorld> Weak(World);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
            [Weak,Started=FPlatformTime::Seconds(),Last=0.0,Step=0,StoppedTime=0.0](float)mutable
        {
            auto* W=Weak.Get();if(!W)return false;
            auto* UI=W->GetSubsystem<UProcessUIStateSubsystem>();
            const double Now=FPlatformTime::Seconds();
            auto Fail=[](const FString& Why){UE_LOG(LogTemp,Error,TEXT("PROCESS_MANUAL_SCREEN_FAIL %s"),*Why);FPlatformMisc::RequestExitWithStatus(false,1);return false;};
            if(Now-Started>180)return Fail(TEXT("wall-clock timeout"));
            if(!UI||!UI->StatusWidget||!UI->ControlService||!UI->ControlUI||Now-Last<1.0)return true;
            auto* Service=UI->ControlService.Get();auto* Widget=UI->StatusWidget.Get();Service->Tick();
            const auto S=Service->State.Snapshot;
            if(S.Validity==EPlantValueValidity::Error)return Fail(S.Error);
            if(S.Validity!=EPlantValueValidity::Valid)return true;
            auto Select=[&](const TCHAR* Id,bool Mixer=false)
            {
                auto* F=Widget->FindFunction(TEXT("SelectEquipment_0"));if(!F)return false;
                FStructOnScope P(F);auto* Data=P.GetStructMemory();
                FString Display=FString(Id).StartsWith(TEXT("supply-"))?FString::Printf(TEXT("공급 호퍼 %c"),TCHAR(FString(Id)[7]-TCHAR('a')+TCHAR('A'))):FString(Id)==TEXT("weigher")?TEXT("칭량호퍼 WH101 출구"):FString(Id)==TEXT("receiver")?TEXT("수취호퍼 RH201"):TEXT("혼합기 MX-301");
                FindFProperty<FStrProperty>(F,TEXT("EquipmentId"))->SetPropertyValue_InContainer(Data,Id);
                for(const TCHAR* N:{TEXT("EquipmentLabelInput"),TEXT("EquipmentDescriptionInput")})
                {if(auto* T=FindFProperty<FTextProperty>(F,N))T->SetPropertyValue_InContainer(Data,FText::FromString(Display));if(auto* T=FindFProperty<FStrProperty>(F,N))T->SetPropertyValue_InContainer(Data,Display);}
                FindFProperty<FBoolProperty>(F,TEXT("IsMixer"))->SetPropertyValue_InContainer(Data,Mixer);
                Widget->ProcessEvent(F,Data);UI->Adapter->Refresh();UI->ControlUI->Refresh();return true;
            };
            auto Click=[&](const TCHAR* Name){auto* B=Cast<UButton>(Widget->GetWidgetFromName(Name));if(!B||!B->GetIsEnabled())return false;B->OnClicked.Broadcast();return true;};
            auto SetRPM=[&](const TCHAR* Value){auto* T=Cast<UEditableTextBox>(Widget->GetWidgetFromName(TEXT("RPMInput")));if(!T)return false;T->SetText(FText::FromString(Value));return Click(TEXT("SetRPM"));};
            auto ObserveViews=[&](){const int32 Before=Service->State.History.Num();for(const TCHAR* N:{TEXT("Show3D"),TEXT("Show2D")}){auto* F=Widget->FindFunction(N);if(!F)return false;FStructOnScope P(F);Widget->ProcessEvent(F,P.GetStructMemory());UI->Adapter->Refresh();}return Service->State.History.Num()==Before;};
            bool Action=true;
            switch(Step)
            {
            case 0:
                if(Service->State.TimeoutPolicy!=EProcessTimeoutPolicy::Freeze || Service->State.PausePolicy!=EProcessPausePolicy::CloseThenPause || Service->State.PausedClosePolicy!=EProcessPausedClosePolicy::StopAll)
                    return Fail(TEXT("ordinary service policy defaults are not active"));
                UI->ControlUI->Refresh();Action=Click(TEXT("GlobalStop"));break;
            case 1:if(Service->State.StopStage!=EProcessStopStage::Complete)return true;StoppedTime=S.PhysicalSeconds;break;
            case 2:
                if(S.PhysicalSeconds!=StoppedTime)return Fail(TEXT("physical time advanced after stop completion"));
                Action=Select(TEXT("mixer"),true)&&SetRPM(TEXT("12garbage"));
                if(Service->State.History[Service->State.LastRequest].Status!=EProcessCommandStatus::Rejected)return Fail(TEXT("invalid numeric suffix accepted"));break;
            case 3:
                {const int32 Before=Service->State.History.Num();Action=Select(TEXT("supply-d"))&&ObserveViews();
                if(Service->State.History.Num()!=Before)return Fail(TEXT("selection or view issued command"));
                auto* B=Widget->GetWidgetFromName(TEXT("ValveOpen"));if(!B||B->GetIsEnabled())return Fail(TEXT("D controls enabled"));
                Action=Select(TEXT("supply-c"))&&Click(TEXT("ManualResume"))&&Action;}break;
            case 4:if(S.Execution!=EPlantExecutionState::Running||S.PhysicalSeconds<=StoppedTime)return true;Action=Click(TEXT("ValveOpen"));break;
            case 5:if(!(S.AppliedSupplierMask&4))return true;Action=Click(TEXT("ValveOpen"))&&Click(TEXT("ValveClose"));break;
            case 6:if(S.AppliedSupplierMask&4)return true;Action=Select(TEXT("weigher"))&&Click(TEXT("ValveOpen"));break;
            case 7:if(!S.AppliedHopperOpen)return true;Action=Click(TEXT("ValveClose"));break;
            case 8:if(S.AppliedHopperOpen)return true;Action=Select(TEXT("receiver"))&&Click(TEXT("ValveOpen"));break;
            case 9:if(!S.AppliedReceiverOpen)return true;Action=Click(TEXT("ValveClose"));break;
            case 10:if(S.AppliedReceiverOpen)return true;Action=Select(TEXT("mixer"),true)&&SetRPM(TEXT("7"));break;
            case 11:if(S.WorkerTargetRPM!=7)return true;if(S.WorkerMixerEnabled)return Fail(TEXT("setpoint started mixer"));Action=Click(TEXT("MixerStart"));break;
            case 12:if(!S.AppliedRPMValid||S.AppliedRPM!=7)return true;Action=Click(TEXT("MixerStop"));break;
            case 13:if(S.AppliedRPM!=0)return true;Action=Click(TEXT("MixerStart"))&&Select(TEXT("supply-c"))&&Click(TEXT("ValveOpen"));break;
            case 14:
                if(S.AppliedRPM!=7 || !(S.AppliedSupplierMask&4))return true;
                Action=Click(TEXT("ManualPause"));
                if(!Service->State.History[Service->State.LastRequest].UsesStopSequence)return Fail(TEXT("manual pause bypassed full stop"));break;
            case 15:
                if(Service->State.StopStage!=EProcessStopStage::Complete)return true;
                if(S.Execution!=EPlantExecutionState::Paused||S.AppliedSupplierMask||S.AppliedHopperOpen||S.AppliedReceiverOpen||S.AppliedRPM!=0)return Fail(TEXT("manual pause completed without worker proof"));
                StoppedTime=S.PhysicalSeconds;break;
            case 16:
                Action=Click(TEXT("ValveClose"));
                if(!Service->State.History[Service->State.LastRequest].UsesStopSequence)return Fail(TEXT("paused close bypassed full stop verification"));break;
            case 17:Action=ObserveViews();break;
            default:
                if(S.PhysicalSeconds!=StoppedTime)return Fail(TEXT("view navigation resumed physics"));
                UE_LOG(LogTemp,Display,TEXT("PROCESS_MANUAL_SCREEN_PASS UMG commands, worker applied values, duplicate open, invalid RPM, D disabled, production defaults, full-stop manual pause, paused close verification, stop/resume, observation without commands"));
                FPlatformMisc::RequestExitWithStatus(false,0);return false;
            }
            if(!Action)return Fail(FString::Printf(TEXT("button/interface unavailable at step %d"),Step));
            UE_LOG(LogTemp,Display,TEXT("PROCESS_MANUAL_SCREEN step=%d epoch=%lld seq=%lld revision=%lld time=%.9g suppliers=%d wh=%d rh=%d rpm=%.3f"),Step,S.Epoch,S.Sequence,S.ProcessedControlRevision,S.PhysicalSeconds,S.AppliedSupplierMask,S.AppliedHopperOpen,S.AppliedReceiverOpen,S.AppliedRPM);
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("ProcessUIControlPolicyValidation")/FString::Printf(TEXT("screen_%02d.png"),Step),true,false);
            ++Step;Last=Now;return true;
        }),.1f);
    }));
}
#endif
