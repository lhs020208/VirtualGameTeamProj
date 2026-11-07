// Development-only, read-only screen acceptance command. No map or asset saves.
#include "ProcessUIStateAdapter.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "PowderPlantTest.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraActor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Input/Events.h"
#include "Widgets/SWindow.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "UnrealClient.h"

namespace
{
// Exercise Slate -> viewport -> controller routing, rather than calling camera movement methods.
FAutoConsoleCommandWithWorldAndArgs NavigationValidation(
    TEXT("ProcessUI.ValidateReadOnlyNavigation"),
    TEXT("Development acceptance: route camera input through Slate, check control isolation, then exit."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
    {
        struct FCheck
        {
            int32 Step=0;
            double Started=FPlatformTime::Seconds(), Last=0;
            FVector Location=FVector::ZeroVector;
            FRotator Rotation=FRotator::ZeroRotator;
            TArray<FProcessEquipmentSnapshot> Before;
        };
        auto Check=MakeShared<FCheck>();
        TWeakObjectPtr<UWorld> WeakWorld(World);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWorld,Check](float)
        {
            UWorld* W=WeakWorld.Get();if(!W)return false;
            auto Fail=[](const TCHAR* Message){UE_LOG(LogTemp,Error,TEXT("PROCESS_UI_NAV_FAIL %s"),Message);FPlatformMisc::RequestExitWithStatus(false,1);};
            const double Now=FPlatformTime::Seconds();
            if(Now-Check->Started>120){Fail(TEXT("UI/source timeout"));return false;}
            auto* Sub=W->GetSubsystem<UProcessUIStateSubsystem>();
            if(!Sub||!Sub->Adapter||!Sub->StatusWidget||Now-Check->Last<.75)return true;
            auto* UI=Sub->StatusWidget.Get();auto* PC=UI->GetOwningPlayer();
            auto* Camera=PC?Cast<ACameraActor>(PC->GetViewTarget()):nullptr;
            if(!Camera){Fail(TEXT("no existing camera view target"));return false;}
            const auto Live=Sub->Adapter->ReadEquipment(TEXT("weigher"));
            if(Live.Validity==EPlantValueValidity::Error){Fail(*Live.Error);return false;}
            if(Live.Validity!=EPlantValueValidity::Valid)return true;
            const TCHAR* Ids[]={TEXT("supply-a"),TEXT("supply-b"),TEXT("supply-c"),TEXT("weigher"),TEXT("receiver"),TEXT("mixer")};
            auto View=[&](bool In3D){auto* F=UI->FindFunction(In3D?TEXT("Show3D"):TEXT("Show2D"));FStructOnScope P(F);UI->ProcessEvent(F,P.GetStructMemory());};
            auto& Slate=FSlateApplication::Get();
            auto Key=[&](FKey K,bool Down){FKeyEvent E(K,FModifierKeysState(),0,false,0,0);if(Down)Slate.ProcessKeyDownEvent(E);else Slate.ProcessKeyUpEvent(E);};
            const auto& Geometry=UI->GetCachedGeometry();
            const FVector2D Pos=Geometry.LocalToAbsolute(Geometry.GetLocalSize()*FVector2D(.85,.85));
            TSet<FKey> Empty,Right;Right.Add(EKeys::RightMouseButton);
            if(Check->Step==0)
            {
                for(const TCHAR* Id:Ids)Check->Before.Add(Sub->Adapter->ReadEquipment(Id));
                Check->Location=Camera->GetActorLocation();View(true);
            }
            else if(Check->Step==1)
            {
                Slate.SetCursorPos(Pos);
                FPointerEvent E(0,Pos,Pos,Empty,EKeys::Invalid,1.f,FModifierKeysState());
                Slate.ProcessMouseMoveEvent(E,true);Slate.ProcessMouseWheelOrGestureEvent(E,nullptr);
            }
            else if(Check->Step==2)
            {
                if(Camera->GetActorLocation().Equals(Check->Location,.01)){Fail(TEXT("wheel input did not move camera through viewport"));return false;}
                UE_LOG(LogTemp,Display,TEXT("PROCESS_UI_NAV wheel moved camera"));
                Check->Location=Camera->GetActorLocation();Check->Rotation=Camera->GetActorRotation();
                auto Window=Slate.FindWidgetWindow(UI->TakeWidget());
                FPointerEvent E(0,Pos,Pos,Right,EKeys::RightMouseButton,0.f,FModifierKeysState());
                Slate.ProcessMouseButtonDownEvent(Window.IsValid()?Window->GetNativeWindow():nullptr,E);
                Key(EKeys::W,true);Key(EKeys::SpaceBar,true);Key(EKeys::R,true);
            }
            else if(Check->Step==3)
            {
                if(Camera->GetActorLocation().Equals(Check->Location,.01)){Fail(TEXT("RMB+W did not move camera"));return false;}
                if(PC->bShowMouseCursor){Fail(TEXT("RMB navigation did not capture the mouse"));return false;}
                // Input bindings enable capture on the next player tick, as with real mouse input.
                // Send relative motion only after that tick has processed the RMB press.
                UE_LOG(LogTemp,Display,TEXT("PROCESS_UI_NAV RMB+W moved camera; capture ready"));
                Slate.OnRawMouseMove(40,15);
            }
            else if(Check->Step==4)
            {
                if(Camera->GetActorRotation().Equals(Check->Rotation,.01)){Fail(TEXT("RMB mouse delta did not rotate camera"));return false;}
                UE_LOG(LogTemp,Display,TEXT("PROCESS_UI_NAV RMB+W and mouse rotation passed"));
                Key(EKeys::SpaceBar,false);Key(EKeys::R,false);
                // Switch with RMB/W still held: cancel capture and accumulated movement.
                View(false);Check->Location=Camera->GetActorLocation();
            }
            else if(Check->Step==5)
            {
                if(!PC->bShowMouseCursor||!Camera->GetActorLocation().Equals(Check->Location,.01)){Fail(TEXT("2D switch left navigation latched"));return false;}
                Key(EKeys::W,false);
                FPointerEvent E(0,Pos,Pos,Empty,EKeys::RightMouseButton,0.f,FModifierKeysState());Slate.ProcessMouseButtonUpEvent(E);
                // Navigation keys/wheel must remain ineffective over the 2D UI.
                FPointerEvent Wheel(0,Pos,Pos,Empty,EKeys::Invalid,1.f,FModifierKeysState());Slate.ProcessMouseWheelOrGestureEvent(Wheel,nullptr);
            }
            else if(Check->Step==6)
            {
                if(!Camera->GetActorLocation().Equals(Check->Location,.01)){Fail(TEXT("2D UI leaked camera input"));return false;}
                for(int32 I=0;I<Check->Before.Num();++I)
                {
                    const auto After=Sub->Adapter->ReadEquipment(Ids[I]);const auto& Before=Check->Before[I];
                    if(Before.Epoch!=After.Epoch||Before.Execution!=After.Execution||Before.ValveRequestedOpen.Value!=After.ValveRequestedOpen.Value||Before.RequestedRPM.Value!=After.RequestedRPM.Value)
                    {Fail(TEXT("camera input changed simulation controls or reset"));return false;}
                }
                if(Live.SimulationTime.Value<=Check->Before[3].SimulationTime.Value){Fail(TEXT("solver time did not advance during navigation"));return false;}
                View(true);
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("ProcessUIStateValidation/navigation.png"),true,false);
                UE_LOG(LogTemp,Display,TEXT("PROCESS_UI_NAV_PASS wheel, RMB+W, rotation, 2D capture cleanup, Space/R blocked, solver %.6f -> %.6f s"),Check->Before[3].SimulationTime.Value,Live.SimulationTime.Value);
            }
            else{FPlatformMisc::RequestExitWithStatus(false,0);return false;}
            ++Check->Step;Check->Last=Now;return true;
        }),.1f);
    }));

FAutoConsoleCommandWithWorldAndArgs ScreenValidation(
    TEXT("ProcessUI.ValidateReadOnlyScreen"),
    TEXT("Development acceptance: select equipment, inspect read-only values, capture UI, then exit. Never saves assets."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
    {
        TWeakObjectPtr<UWorld> WeakWorld(World);
        const double Started=FPlatformTime::Seconds();
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWorld,Started,Step=0,LastStep=0.0](float) mutable
        {
            UWorld* W=WeakWorld.Get();
            if(!W)return false;
            auto* Sub=W->GetSubsystem<UProcessUIStateSubsystem>();
            const double Now=FPlatformTime::Seconds();
            auto Fail=[](const TCHAR* Message){UE_LOG(LogTemp,Error,TEXT("PROCESS_UI_SCREEN_FAIL %s"),Message);FPlatformMisc::RequestExitWithStatus(false,1);};
            if(Now-Started>150){Fail(TEXT("UI/source timeout"));return false;}
            if(!Sub||!Sub->Adapter||!Sub->StatusWidget||Now-LastStep<2.0)return true;
            auto* UI=Sub->StatusWidget.Get();
            const auto Live=Sub->Adapter->ReadEquipment(TEXT("weigher"));
            if(Live.Validity==EPlantValueValidity::Error){Fail(*Live.Error);return false;}
            if(Live.Validity!=EPlantValueValidity::Valid)return true;
            const TCHAR* Ids[]={TEXT("supply-a"),TEXT("supply-b"),TEXT("supply-c"),TEXT("supply-d"),TEXT("supply-e"),TEXT("supply-f"),TEXT("weigher"),TEXT("receiver"),TEXT("mixer")};
            const TCHAR* Labels[]={TEXT("공급 호퍼 A"),TEXT("공급 호퍼 B"),TEXT("공급 호퍼 C"),TEXT("공급 호퍼 D"),TEXT("공급 호퍼 E"),TEXT("공급 호퍼 F"),TEXT("칭량호퍼"),TEXT("수취호퍼"),TEXT("혼합기")};
            TArray<FProcessEquipmentSnapshot> Before;
            for(const TCHAR* Id:Ids)Before.Add(Sub->Adapter->ReadEquipment(Id));
            if(Step<9)
            {
                UFunction* F=UI->FindFunction(TEXT("SelectEquipment_0"));
                if(!F){Fail(TEXT("selection function missing"));return false;}
                FStructOnScope P(F);uint8* Data=P.GetStructMemory();
                FindFProperty<FStrProperty>(F,TEXT("EquipmentId"))->SetPropertyValue_InContainer(Data,Ids[Step]);
                for(const TCHAR* Name:{TEXT("EquipmentLabelInput"),TEXT("EquipmentDescriptionInput")})
                {
                    if(auto* Text=FindFProperty<FTextProperty>(F,Name))Text->SetPropertyValue_InContainer(Data,FText::FromString(Labels[Step]));
                    if(auto* String=FindFProperty<FStrProperty>(F,Name))String->SetPropertyValue_InContainer(Data,Labels[Step]);
                }
                FindFProperty<FBoolProperty>(F,TEXT("IsMixer"))->SetPropertyValue_InContainer(Data,Step==8);
                UI->ProcessEvent(F,Data);Sub->Adapter->Refresh();
                const auto State=Sub->Adapter->ReadEquipment(Ids[Step]);
                auto* Status=Cast<UTextBlock>(UI->GetWidgetFromName(TEXT("StatusValue")));
                auto* Quantity=Cast<UTextBlock>(UI->GetWidgetFromName(TEXT("QuantityValue")));
                auto* Valve=Cast<UTextBlock>(UI->GetWidgetFromName(TEXT("ValveValue")));
                if(!Status||!Quantity||!Valve){Fail(TEXT("status widgets missing"));return false;}
                if(Step>=3&&Step<=5&&Status->GetText().ToString()!=TEXT("시뮬레이션 미구현")){Fail(TEXT("D/E/F incorrectly mapped"));return false;}
                if(Step==2&&(State.InitialParticleCount.Validity!=EPlantValueValidity::Valid||State.InitialParticleCount.Value!=0)){Fail(TEXT("C initial charge must be valid zero"));return false;}
                UE_LOG(LogTemp,Display,TEXT("PROCESS_UI_SCREEN %s | %s | %s | %s | epoch=%lld seq=%lld"),Ids[Step],*Status->GetText().ToString(),*Quantity->GetText().ToString(),*Valve->GetText().ToString(),State.Epoch,State.Sequence);
            }
            else if(Step<11)
            {
                UFunction* F=UI->FindFunction(Step==9?TEXT("Show3D"):TEXT("Show2D"));
                FStructOnScope P(F);UI->ProcessEvent(F,P.GetStructMemory());Sub->Adapter->Refresh();
                UE_LOG(LogTemp,Display,TEXT("PROCESS_UI_SCREEN view=%s"),Step==9?TEXT("3D"):TEXT("2D"));
            }
            else
            {
                UE_LOG(LogTemp,Display,TEXT("PROCESS_UI_SCREEN_PASS 9 equipment, 2D/3D, commands disabled, no control writes"));
                FPlatformMisc::RequestExitWithStatus(false,0);return false;
            }
            for(const TCHAR* Name:{TEXT("ValveOpen"),TEXT("ValveClose"),TEXT("TransferStart"),TEXT("TransferStop"),TEXT("MixerStart"),TEXT("MixerStop"),TEXT("SetRPM"),TEXT("ManualRequest")})
            {
                auto* Button=UI->GetWidgetFromName(Name);
                if(!Button||Button->GetIsEnabled()){Fail(TEXT("a command was enabled"));return false;}
            }
            for(int32 I=0;I<9;++I)
            {
                const auto After=Sub->Adapter->ReadEquipment(Ids[I]);
                if(Before[I].RequestedRPM.Value!=After.RequestedRPM.Value||Before[I].Epoch!=After.Epoch||
                    Before[I].ValveRequestedOpen.Value!=After.ValveRequestedOpen.Value||Before[I].Execution!=After.Execution)
                {Fail(TEXT("UI changed simulation control/reset"));return false;}
            }
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("ProcessUIStateValidation")/FString::Printf(TEXT("screen_%02d.png"),Step),true,false);
            ++Step;LastStep=Now;return true;
        }),.25f);
    }));
}
#endif
