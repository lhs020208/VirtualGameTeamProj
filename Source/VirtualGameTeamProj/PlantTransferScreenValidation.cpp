#include "ProcessUIStateAdapter.h"
#include "ProcessControlService.h"
#include "ProcessControlUI.h"
#include "PowderPlantTest.h"
#include "ProcessPlantBinding.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"
#include "Misc/FileHelper.h"
namespace
{
// Isolated development game process: drives the existing manual UMG delegates, never edits physics/input data.
void ValidateTransferScreen(UWorld* World,bool ExitAfter,double Limit)
{
    const bool LowPoly=UWorld::RemovePIEPrefix(World->GetOutermost()->GetName())==ProcessPlantBinding::LowPolyMap;
    const TWeakObjectPtr<UWorld> Weak(World);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
        [Weak,Limit,ExitAfter,LowPoly,Start=FPlatformTime::Seconds(),LastLog=0.0,Step=0,SettledSince=-1.0,LastMass=-1.0,Frozen=0.0,OldEpoch=int64(0),SawPipe=false,SawArrival=false](float)mutable
    {
        auto* W=Weak.Get();if(!W)return false;
        auto* UI=W->GetSubsystem<UProcessUIStateSubsystem>();if(!UI||!UI->ControlService||!UI->ControlUI||!UI->StatusWidget)return true;
        auto* Service=UI->ControlService.Get();auto* Widget=UI->StatusWidget.Get();Service->Tick();UI->Adapter->Refresh();UI->ControlUI->Refresh();
        const auto S=Service->State.Snapshot;const double Now=FPlatformTime::Seconds();
        const FString Out=FPaths::ProjectSavedDir()/(LowPoly?TEXT("LowPolyMassTransferValidation"):TEXT("ProcessUITransferValidation"));
        auto Capture=[&](const FString& Name){FScreenshotRequest::RequestScreenshot(Out/Name,true,false);};
        auto End=[&](const TCHAR* Result,const FString& Why,int32 Code){UE_LOG(LogTemp,Display,TEXT("PROCESS_TRANSFER_SCREEN_%s step=%d time=%.6f id=%lld baseline_kg=%.9g remaining_kg=%.9g pipe_kg=%.9g arrived_kg=%.9g unclassified_kg=%.9g %s"),Result,Step,S.PhysicalSeconds,S.Transfer.Id,S.Transfer.BaselineKg,S.Transfer.RemainingKg,S.Transfer.InTransitKg,S.Transfer.ArrivedKg,S.Transfer.UnclassifiedKg,*Why);Capture(TEXT("final.png"));
            FFileHelper::SaveStringToFile(FString::Printf(TEXT("%s\n%s\ntime=%.9g id=%lld baseline=%.12g remaining=%.12g transit=%.12g arrived=%.12g unknown=%.12g"),Result,*Why,S.PhysicalSeconds,S.Transfer.Id,S.Transfer.BaselineKg,S.Transfer.RemainingKg,S.Transfer.InTransitKg,S.Transfer.ArrivedKg,S.Transfer.UnclassifiedKg),*(Out/TEXT("result.txt")));
            if(ExitAfter)FPlatformMisc::RequestExitWithStatus(false,Code);
            else Service->Request(TEXT("plant"),EProcessCommand::StopAll,0);
            return false;};
        if(Now-LastLog>=10)
        {
            UE_LOG(LogTemp,Display,TEXT("PROCESS_TRANSFER_OBSERVE wall=%.1f step=%d epoch=%lld time=%.6f wh_kg=%.9g pipe_kg=%.9g rh_kg=%.9g id=%lld baseline=%.9g remaining=%.9g transit=%.9g arrived=%.9g state=%d stable=%.6f supplier_applied=%d wh_requested=%d wh_applied=%d manual=%d reason=%s"),
                Now-Start,Step,S.Epoch,S.PhysicalSeconds,S.WeighingMass.Kg,S.PipeMass.Kg,S.ReceiverMass.Kg,S.Transfer.Id,S.Transfer.BaselineKg,S.Transfer.RemainingKg,S.Transfer.InTransitKg,S.Transfer.ArrivedKg,int32(S.Transfer.State),S.Transfer.StableSeconds,S.AppliedSupplierMask,S.RequestedHopperOpen,S.AppliedHopperOpen,Service->State.ManualGranted,*S.Transfer.Reason);
            FFileHelper::SaveStringToFile(FString::Printf(TEXT("%.3f,%d,%lld,%.9g,%.12g,%.12g,%.12g,%lld,%.12g,%.12g,%.12g,%.12g,%.12g,%d,%.9g,%d,%d,%.9g,%s\n"),Now-Start,Step,S.Epoch,S.PhysicalSeconds,S.WeighingMass.Kg,S.PipeMass.Kg,S.ReceiverMass.Kg,S.Transfer.Id,S.Transfer.BaselineKg,S.Transfer.RemainingKg,S.Transfer.InTransitKg,S.Transfer.ArrivedKg,S.Transfer.UnclassifiedKg,int32(S.Transfer.State),S.Transfer.StableSeconds,S.AppliedSupplierMask,S.AppliedHopperOpen,S.Transfer.CalculatedCarrierFlowM3s,*S.Transfer.Reason),*(Out/TEXT("observations.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);
            if(!LowPoly||int32((Now-Start)/10)%6==0)Capture(FString::Printf(TEXT("observe_%03d.png"),int32((Now-Start)/10)));LastLog=Now;
        }
        if(Now-Start>Limit)return End(TEXT("LIMIT"),TEXT("Observation wall limit reached. No claim of actual GPU transfer completion."),2);
        if(S.Validity==EPlantValueValidity::Error)return End(TEXT("ERROR"),S.Error,1);
        if(S.Validity!=EPlantValueValidity::Valid||S.WeighingMass.Validity!=EPlantValueValidity::Valid)return true;
        auto Select=[&](const TCHAR* Id)
        {
            auto* F=Widget->FindFunction(TEXT("SelectEquipment_0"));if(!F)return false;FStructOnScope P(F);auto* Data=P.GetStructMemory();
            FindFProperty<FStrProperty>(F,TEXT("EquipmentId"))->SetPropertyValue_InContainer(Data,Id);
            const FString Display=FString(Id)==TEXT("weigher")?TEXT("칭량호퍼 WH101 출구"):FString(Id)==TEXT("supply-a")?TEXT("공급 호퍼 A"):FString(Id);
            for(const TCHAR* N:{TEXT("EquipmentLabelInput"),TEXT("EquipmentDescriptionInput")})
            {if(auto* T=FindFProperty<FTextProperty>(F,N))T->SetPropertyValue_InContainer(Data,FText::FromString(Display));if(auto* T=FindFProperty<FStrProperty>(F,N))T->SetPropertyValue_InContainer(Data,Display);}
            FindFProperty<FBoolProperty>(F,TEXT("IsMixer"))->SetPropertyValue_InContainer(Data,false);Widget->ProcessEvent(F,Data);UI->Adapter->Refresh();UI->ControlUI->Refresh();return true;
        };
        auto Click=[&](const TCHAR* Name){auto* B=Cast<UButton>(Widget->GetWidgetFromName(Name));if(!B||!B->GetIsEnabled())return false;B->OnClicked.Broadcast();return true;};
        bool Action=true;
        switch(Step)
        {
        case 0:
            FFileHelper::SaveStringToFile(TEXT("wall_s,step,epoch,simulation_s,wh_kg,pipe_kg,rh_kg,id,baseline_kg,remaining_kg,transit_kg,arrived_kg,unclassified_kg,state,stable_s,suppliers,wh_open,carrier_m3s,reason\n"),*(Out/TEXT("observations.csv")));
            Action=Select(TEXT("weigher"))&&Click(TEXT("GlobalStop"));break;
        case 1:if(Service->State.StopStage!=EProcessStopStage::Complete)return true;Frozen=S.PhysicalSeconds;SettledSince=Now;break;
        case 2:
            if(Now-SettledSince<1)return true;
            if(S.PhysicalSeconds!=Frozen||S.WeighingMass.Kg!=0)return End(TEXT("FAIL"),TEXT("initial paused empty mass/time invalid"),1);
            Action=Click(TEXT("ManualResume"));break;
        case 3:if(S.Execution!=EPlantExecutionState::Running)return true;Action=Click(TEXT("ValveOpen"));break;
        case 4:
            if(S.Transfer.Id==0)return true;
            if(S.Transfer.State!=EPlantTransferState::Empty)return End(TEXT("FAIL"),TEXT("initial empty opening was not classified empty"),1);
            Capture(TEXT("empty.png"));Action=Click(TEXT("ValveClose"));break;
        case 5:if(S.AppliedHopperOpen)return true;Action=Select(TEXT("supply-a"))&&Click(TEXT("ValveOpen"));break;
        case 6:
            // Development staging only: request inlet closure once material is present;
            // actual closure still waits for the existing worker's occupancy guard.
            if(S.WeighingMass.Kg<.0004)return true;
            Action=Click(TEXT("ValveClose"))&&Select(TEXT("weigher"));break;
        case 7:
            if(S.AppliedSupplierMask||S.RequestedSuppliers[0])return true;
            // Restore confirmed manual permission if the standalone close exceeded its command acknowledgement deadline.
            if(!Service->State.ManualGranted){Action=Click(TEXT("GlobalStop"));Step=8;return Action?true:End(TEXT("FAIL"),TEXT("manual entry retry unavailable"),1);}
            Step=9;SettledSince=S.PhysicalSeconds;LastMass=S.WeighingMass.Kg;return true;
        case 8:
            if(Service->State.StopStage==EProcessStopStage::Failed)return End(TEXT("STOP_FAILED"),Service->State.StopDescription(),2);
            if(Service->State.StopStage!=EProcessStopStage::Complete)return true;
            Action=Click(TEXT("ManualResume"));SettledSince=S.PhysicalSeconds;LastMass=S.WeighingMass.Kg;break;
        case 9:
            if(!Service->State.ManualGranted)return End(TEXT("ACCESS_LOST"),FString::Printf(TEXT("Manual permission lost; snapshot validity=%d age=%.6f; no opening was issued"),int32(S.Validity),Now-S.PublishedWallSeconds),2);
            if(S.WeighingMass.Kg!=LastMass){LastMass=S.WeighingMass.Kg;SettledSince=S.PhysicalSeconds;}
            // Test staging only: allow delayed upstream grains to settle before manually opening the outlet.
            // This does not relax the production completion criteria or suppress concurrent-inflow detection.
            if(S.Execution!=EPlantExecutionState::Running||S.PhysicalSeconds-SettledSince<1.0)return true;
            if(LowPoly){Action=Select(TEXT("supply-f"))&&Click(TEXT("ValveOpen"));Step=20;return Action?true:End(TEXT("FAIL"),TEXT("empty F carrier branch unavailable"),1);}
            Action=Select(TEXT("weigher"))&&Click(TEXT("ValveOpen"));break;
        case 20:
            if(!(S.AppliedSupplierMask&32))return true;
            Action=Select(TEXT("weigher"))&&Click(TEXT("ValveOpen"));Step=10;
            return Action?true:End(TEXT("FAIL"),TEXT("WH manual opening unavailable"),1);
        case 10:
            if(S.Transfer.Id<2)return true;
            if(S.Transfer.InTransitKg>0){SawPipe=true;if(S.Transfer.State==EPlantTransferState::Complete)return End(TEXT("FAIL"),TEXT("completed with pipe residue"),1);}
            if(S.Transfer.ArrivedKg>0)SawArrival=true;
            if(S.Transfer.State==EPlantTransferState::Indeterminate)return End(TEXT("INDETERMINATE"),S.Transfer.Reason,2);
            if(S.Transfer.State!=EPlantTransferState::Complete)return true;
            Capture(TEXT("complete.png"));Action=Click(TEXT("ManualPause"));break;
        case 11:
            if(Service->State.StopStage!=EProcessStopStage::Complete)return true;
            Frozen=S.PhysicalSeconds;SettledSince=Now;break;
        case 12:
            if(Now-SettledSince<1)return true;
            if(S.PhysicalSeconds!=Frozen)return End(TEXT("FAIL"),TEXT("manual pause advanced physics"),1);
            OldEpoch=S.Epoch;Service->GetSource()->ResetSimulation();break;
        default:
            if(S.Epoch<=OldEpoch)return true;
            if(S.Transfer.Id!=0||S.WeighingMass.Kg!=0)return End(TEXT("FAIL"),TEXT("reset retained prior cohort/mass"),1);
            return End(TEXT("PASS"),FString::Printf(TEXT("original input; UMG manual commands; empty, mass, pipe_seen=%d arrival_seen=%d, completion, pause, reset"),SawPipe,SawArrival),0);
        }
        if(!Action)return End(TEXT("FAIL"),TEXT("manual button unavailable"),1);
        Capture(FString::Printf(TEXT("step_%02d.png"),Step));++Step;return true;
    }),.1f);
 }
FAutoConsoleCommandWithWorld TransferScreenTest(TEXT("ProcessUI.ValidateTransferScreen"),TEXT("Observe original FlowTest manual mass transfer and exit dedicated process."),
FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World){double Limit=900;FParse::Value(FCommandLine::Get(),TEXT("PlantTransferObservationWall="),Limit);ValidateTransferScreen(World,true,Limit);}));
FAutoConsoleCommandWithWorldAndArgs LowPolyTransferScreenTest(TEXT("ProcessUI.ValidateLowPolyTransferPIE"),TEXT("Explicit QA only: drive existing manual UMG controls and observe the original LowPoly GPU charge; optional wall limit seconds."),
FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World){
    if(!World||World->WorldType!=EWorldType::PIE||UWorld::RemovePIEPrefix(World->GetOutermost()->GetName())!=ProcessPlantBinding::LowPolyMap)return;
    const double Limit=Args.Num()?FCString::Atod(*Args[0]):1800;
    if(FMath::IsFinite(Limit)&&Limit>0)ValidateTransferScreen(World,false,Limit);
}));
}
#endif
