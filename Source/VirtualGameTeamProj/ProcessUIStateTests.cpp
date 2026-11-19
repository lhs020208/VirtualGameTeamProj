#include "ProcessUIStateAdapter.h"
#include "PowderPlantTest.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcessStateMappingTest, "ProcessUI.ReadOnly.MappingAndFreshness", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProcessStateMappingTest::RunTest(const FString&)
{
    FPlantSimulationSnapshot S;
    S.Epoch=2; S.Sequence=7; S.PublishedWallSeconds=100; S.Validity=EPlantValueValidity::Valid;
    S.Execution=EPlantExecutionState::Running; S.PhysicalSeconds=12.5;
    S.RequestedSuppliers={true,false,true}; S.AppliedSupplierMask=2; S.InitialSupplierCounts={10000,10000,0};
    S.InWeighingHopper=11; S.InReceiver=22; S.InMixer=33;
    S.RequestedHopperOpen=true; S.AppliedHopperOpen=false;
    S.RequestedReceiverOpen=false; S.AppliedReceiverOpen=true;
    S.RequestedRPM=15; S.AppliedRPM=0; S.AppliedRPMValid=true;
    auto Read=[&](const TCHAR* Id){return UProcessUIStateAdapter::BuildEquipment(Id,&S,101);};
    const auto A=Read(TEXT("supply-a")),B=Read(TEXT("supply-b")),C=Read(TEXT("supply-c"));
    TestEqual(TEXT("A requested gate 0"),A.ValveRequestedOpen.Value,1.0);
    TestEqual(TEXT("A applied gate 0"),A.ValveAppliedOpen.Value,0.0);
    TestEqual(TEXT("B applied gate 1"),B.ValveAppliedOpen.Value,1.0);
    TestEqual(TEXT("C requested gate 2"),C.ValveRequestedOpen.Value,1.0);
    TestEqual(TEXT("C supported empty initial charge"),C.InitialParticleCount.Value,0.0);
    TestTrue(TEXT("C is implemented"),C.Validity==EPlantValueValidity::Valid);
    TestTrue(TEXT("supplier initial count never current inventory"),A.RegionParticleCount.Validity==EPlantValueValidity::Unknown);
    for(const TCHAR* Id:{TEXT("supply-d"),TEXT("supply-e"),TEXT("supply-f")})
        TestTrue(Id,Read(Id).Validity==EPlantValueValidity::Unsupported);
    auto W=Read(TEXT("weigher")),R=Read(TEXT("receiver")),M=Read(TEXT("mixer"));
    TestEqual(TEXT("WH region"),W.RegionParticleCount.Value,11.0);
    TestEqual(TEXT("RH region"),R.RegionParticleCount.Value,22.0);
    TestEqual(TEXT("MX region"),M.RegionParticleCount.Value,33.0);
    TestEqual(TEXT("WH outlet requested, not BV122"),W.ValveRequestedOpen.Value,1.0);
    TestEqual(TEXT("RH outlet applied"),R.ValveAppliedOpen.Value,1.0);
    TestEqual(TEXT("RPM setpoint retained"),M.RequestedRPM.Value,15.0);
    TestEqual(TEXT("applied RPM never replaced by target"),M.AppliedRPM.Value,0.0);
    S.AppliedRPMValid=false;
    TestTrue(TEXT("unknown applied RPM"),Read(TEXT("mixer")).AppliedRPM.Validity==EPlantValueValidity::Unknown);
    auto Stale=UProcessUIStateAdapter::BuildEquipment(TEXT("weigher"),&S,103);
    TestTrue(TEXT("stale clock uses wall time"),Stale.Validity==EPlantValueValidity::Stale);
    TestTrue(TEXT("missing actor"),UProcessUIStateAdapter::BuildEquipment(TEXT("weigher"),nullptr,101).Validity==EPlantValueValidity::SourceMissing);
    S.Validity=EPlantValueValidity::Error;S.Error=TEXT("test fault");
    TestTrue(TEXT("fault distinct from stale"),UProcessUIStateAdapter::BuildEquipment(TEXT("mixer"),&S,1000).Validity==EPlantValueValidity::Error);
    FText Status,Quantity,Valve;
    UProcessUIStateAdapter::Format(W,Status,Quantity,Valve);
    UProcessUIStateAdapter::Format(Read(TEXT("supply-d")),Status,Quantity,Valve);
    TestEqual(TEXT("unsupported label"),Status.ToString(),FString(TEXT("시뮬레이션 미구현")));
    TestEqual(TEXT("selection clears previous quantity"),Quantity.ToString(),FString(TEXT("미확인")));
    TestEqual(TEXT("selection clears previous valve"),Valve.ToString(),FString(TEXT("미확인")));
    FProcessSnapshotCursor Cursor;
    TestTrue(TEXT("first sequence"),Cursor.Accept(2,10));
    TestTrue(TEXT("reset with smaller sequence accepted"),Cursor.Accept(3,1));
    TestFalse(TEXT("previous epoch rejected"),Cursor.Accept(2,999));
    TestFalse(TEXT("out of order rejected"),Cursor.Accept(3,0));
    TestTrue(TEXT("same result may refresh multiple equipment"),Cursor.Accept(3,1));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcessWidgetBindingTest, "ProcessUI.ReadOnly.WidgetBinding", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProcessWidgetBindingTest::RunTest(const FString&)
{
    UWorld::InitializationValues IV;
    IV.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);
    if(!TestNotNull(TEXT("test world"),World))return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    auto* Player=NewObject<ULocalPlayer>(GEngine);
    auto* Controller=World->SpawnActor<APlayerController>();
    Controller->SetPlayer(Player);
    auto* Class=LoadClass<UUserWidget>(nullptr,TEXT("/Game/ProcessUIV2/Widgets/WBP_ProcessUIV2.WBP_ProcessUIV2_C"));
    auto* Widget=Class?CreateWidget<UUserWidget>(Controller,Class):nullptr;
    if(!TestNotNull(TEXT("real UI_v2 asset"),Widget)){World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false;}
    auto* Adapter=NewObject<UProcessUIStateAdapter>(World);
    TestTrue(TEXT("actual Blueprint interface attached"),Adapter->Attach(Widget,nullptr));
    auto Text=[&](const TCHAR* Name){auto* B=Cast<UTextBlock>(Widget->GetWidgetFromName(Name));return B?B->GetText().ToString():FString();};
    auto Select=[&](const TCHAR* Id){
        UFunction* F=Widget->FindFunction(TEXT("SelectEquipment_0"));
        if(!TestNotNull(TEXT("selection graph"),F))return;
        FStructOnScope Params(F);
        FindFProperty<FStrProperty>(F,TEXT("EquipmentId"))->SetPropertyValue_InContainer(Params.GetStructMemory(),Id);
        Widget->ProcessEvent(F,Params.GetStructMemory());Adapter->Refresh();
    };
    Select(TEXT("weigher"));
    TestEqual(TEXT("missing source rendered"),Text(TEXT("StatusValue")),FString(TEXT("시뮬레이션에 연결되지 않았습니다")));
    Select(TEXT("supply-d"));
    TestEqual(TEXT("D unsupported rendered"),Text(TEXT("StatusValue")),FString(TEXT("시뮬레이션 미구현")));
    Select(TEXT("supply-c"));
    TestEqual(TEXT("C missing source, not unsupported"),Text(TEXT("StatusValue")),FString(TEXT("시뮬레이션에 연결되지 않았습니다")));
    for(const TCHAR* Name:{TEXT("ValveOpen"),TEXT("ValveClose"),TEXT("TransferStart"),TEXT("TransferStop"),TEXT("MixerStart"),TEXT("MixerStop"),TEXT("SetRPM"),TEXT("ManualRequest")})
    {
        auto* Button=Widget->GetWidgetFromName(Name);
        TestTrue(Name,Button&&!Button->GetIsEnabled());
    }
    const FString Selected=FindFProperty<FStrProperty>(Widget->GetClass(),TEXT("SelectedEquipmentId"))->GetPropertyValue_InContainer(Widget);
    for(const TCHAR* Name:{TEXT("Show3D"),TEXT("Show2D")})
        if(UFunction* F=Widget->FindFunction(Name)){FStructOnScope Params(F);Widget->ProcessEvent(F,Params.GetStructMemory());}
    TestEqual(TEXT("2D/3D preserves selection"),FindFProperty<FStrProperty>(Widget->GetClass(),TEXT("SelectedEquipmentId"))->GetPropertyValue_InContainer(Widget),Selected);
    TestEqual(TEXT("source is honestly labelled"),Text(TEXT("DataSource")),FString(TEXT("실시간 시뮬레이션 · 미보정")));
    Adapter->Detach();TestFalse(TEXT("unbound on close"),Adapter->IsAttached());
    World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcessWorkerSnapshotTest, "ProcessUI.ReadOnly.WorkerSnapshot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProcessWorkerSnapshotTest::RunTest(const FString&)
{
    UWorld::InitializationValues IV;
    IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);
    if(!TestNotNull(TEXT("worker test world"),World))return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    auto* Actor=World->SpawnActor<APowderPlantTest>();
    Actor->Running=false;
    Actor->DispatchBeginPlay();
    auto Wait=[&](TFunctionRef<bool(const FPlantSimulationSnapshot&)> Ready){
        double Deadline=FPlatformTime::Seconds()+45;
        while(FPlatformTime::Seconds()<Deadline){auto S=Actor->GetSimulationSnapshot();if(S.Validity==EPlantValueValidity::Error){AddError(S.Error);return false;}if(Ready(S))return true;FPlatformProcess::Sleep(.02f);}
        AddError(TEXT("worker telemetry timeout"));return false;
    };
    bool Ready=Wait([](const auto& S){return S.Validity==EPlantValueValidity::Valid&&S.Execution==EPlantExecutionState::Paused;});
    if(Ready)
    {
        const auto Initial=Actor->GetSimulationSnapshot();
        TestEqual(TEXT("paused starts at zero time"),Initial.PhysicalSeconds,0.0);
        TestFalse(TEXT("no fabricated initial applied RPM"),Initial.AppliedRPMValid);
        TestEqual(TEXT("supplier count"),Initial.RequestedSuppliers.Num(),3);
        TestEqual(TEXT("C valid initial empty"),Initial.InitialSupplierCounts[2],0);
        for(int I=0;I<100;++I)Actor->GetSimulationSnapshot();
        TestFalse(TEXT("queries never start simulation"),Actor->Running);
        Actor->Running=true;Actor->Tick(.01f);
        Ready=Wait([](const auto& S){return S.AppliedRPMValid&&S.PhysicalSeconds>0;});
        Actor->Running=false;Actor->Tick(.01f);
        Ready=Wait([](const auto& S){return S.Execution==EPlantExecutionState::Paused;})&&Ready;
        const auto Paused=Actor->GetSimulationSnapshot();
        FPlatformProcess::Sleep(.25f);
        const auto Heartbeat=Actor->GetSimulationSnapshot();
        TestEqual(TEXT("heartbeat never advances physics"),Heartbeat.PhysicalSeconds,Paused.PhysicalSeconds);
        TestTrue(TEXT("paused worker still confirms freshness"),Heartbeat.Sequence>Paused.Sequence);
        TestEqual(TEXT("disabled mixer applied zero, not requested 15"),Heartbeat.AppliedRPM,0.0);
        TestEqual(TEXT("requested setpoint remains 15"),Heartbeat.RequestedRPM,15.0);
        const auto OldEpoch=Heartbeat.Epoch;
        Actor->ResetSimulation();
        auto Reset=Actor->GetSimulationSnapshot();
        TestTrue(TEXT("reset advances epoch synchronously"),Reset.Epoch>OldEpoch);
        TestEqual(TEXT("old physical time never leaks after reset"),Reset.PhysicalSeconds,0.0);
        TestFalse(TEXT("reset does not turn running on"),Actor->Running);
        Wait([](const auto& S){return S.Validity==EPlantValueValidity::Valid&&S.Execution==EPlantExecutionState::Paused;});
    }
    Actor->Destroy();World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
    return Ready;
}
#endif
