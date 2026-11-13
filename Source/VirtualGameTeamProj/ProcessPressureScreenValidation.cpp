#include "ProcessUIStateAdapter.h"
#include "ProcessControlService.h"
#include "ProcessControlUI.h"
#include "ProcessPlantBinding.h"
#include "PowderPlantTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"
#include "UnrealClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

namespace {
// Explicit opt-in QA only. Original level/charge/criteria; drives existing UMG manual controls.
void PressurePIE(UWorld* World)
{
    const TWeakObjectPtr<UWorld> Weak(World);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
        [Weak,Start=FPlatformTime::Seconds(),LastLog=0.,Step=0,Entered=0.,StageWall=0.,Frozen=0.,Mark=0.,Stable=0.,LastTransit=-1.,
         OldEpoch=int64(0),BeforeRevision=int64(0),BeforeHistory=0,LastRequest=FGuid(),OldRequest=FGuid(),SawPipe=false,SawArrival=false,Stopping=false,StopShot=false](float)mutable
    {
        auto* W=Weak.Get();if(!W)return false;
        auto* UI=W->GetSubsystem<UProcessUIStateSubsystem>();if(!UI||!UI->ControlService||!UI->ControlUI||!UI->StatusWidget)return true;
        auto* Service=UI->ControlService.Get();auto* Widget=UI->StatusWidget.Get();Service->Tick();UI->Adapter->Refresh();UI->ControlUI->Refresh();
        auto& M=Service->State;const auto S=M.Snapshot;const double Now=FPlatformTime::Seconds();
        const FString Out=FPaths::ProjectSavedDir()/(FParse::Param(FCommandLine::Get(),TEXT("PressureUIProtocolOnly"))?TEXT("PressureUIIntegration/ProtocolPIE"):TEXT("PressureUIIntegration"));
        auto Append=[&](const TCHAR* Name,const FString& Text){FFileHelper::SaveStringToFile(Text,*(Out/Name),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);};
        auto Capture=[&](const FString& Name){
            // Do not leave a screenshot/readback pending across a minimized editor's swap-chain restore.
            // This is a QA capture guard, not a claim to fix the observed D3D12/Slate renderer crash.
            if(!FSlateApplication::IsInitialized())return;
            const auto Window=FSlateApplication::Get().FindWidgetWindow(Widget->TakeWidget());
            if(Window.IsValid()&&!Window->IsWindowMinimized())FScreenshotRequest::RequestScreenshot(Out/Name,true,false);
        };
        auto End=[&](const TCHAR* Result,const FString& Why){
            FFileHelper::SaveStringToFile(FString::Printf(TEXT("%s\n%s\nwall_s=%.6f simulation_s=%.9g step=%d epoch=%lld\nrequested_pa=%s applied_pa=%s supplier_mask=%d wh_open=%d rh_open=%d\ncohort_id=%lld baseline_kg=%.12g remaining_kg=%.12g transit_kg=%.12g arrived_kg=%.12g unknown_kg=%.12g state=%d\nreason=%s\npipe_seen=%d arrival_seen=%d stop=%s\n"),
                Result,*Why,Now-Start,S.PhysicalSeconds,Step,S.Epoch,*S.RequestedGaugePressurePa.ToString(),*S.AppliedGaugePressurePa.ToString(),S.AppliedSupplierMask,S.AppliedHopperOpen,S.AppliedReceiverOpen,
                S.Transfer.Id,S.Transfer.BaselineKg,S.Transfer.RemainingKg,S.Transfer.InTransitKg,S.Transfer.ArrivedKg,S.Transfer.UnclassifiedKg,int32(S.Transfer.State),*S.Transfer.Reason,SawPipe,SawArrival,*M.StopDescription()),*(Out/TEXT("pie_result.txt")));
            UE_LOG(LogTemp,Display,TEXT("PRESSURE_PIE_%s step=%d time=%.6f %s"),Result,Step,S.PhysicalSeconds,*Why);Capture(TEXT("pie_final.png"));
            if(!Stopping&&S.Execution!=EPlantExecutionState::Fault)Service->Request(TEXT("plant"),EProcessCommand::StopAll,0);
            return false;
        };
        if(Now-LastLog>=5||Entered==0)
        {
            if(Entered==0){Entered=StageWall=Now;IFileManager::Get().MakeDirectory(*Out,true);
                FFileHelper::SaveStringToFile(TEXT("wall_s,step,epoch,seq,simulation_s,request_supply_pa,request_wh_pa,request_rh_pa,applied_supply_pa,applied_wh_pa,applied_rh_pa,pressure_rev,processed_rev,supplier_mask,wh_open,rh_open,rpm,wh_kg,pipe_kg,rh_kg,id,baseline_kg,remaining_kg,transit_kg,arrived_kg,unknown_kg,state,flow_m3s,regulator_m3s,reason\n"),*(Out/TEXT("pie_observations.csv")));
                FFileHelper::SaveStringToFile(TEXT("wall_s,simulation_s,step,event\n"),*(Out/TEXT("pie_events.csv")));}
            FString Reason=S.Transfer.Reason.Replace(TEXT("\n"),TEXT(" ")).Replace(TEXT(","),TEXT(";"));
            Append(TEXT("pie_observations.csv"),FString::Printf(TEXT("%.3f,%d,%lld,%lld,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%lld,%lld,%d,%d,%d,%.9g,%.12g,%.12g,%.12g,%lld,%.12g,%.12g,%.12g,%.12g,%.12g,%d,%.12g,%.12g,%s\n"),
                Now-Start,Step,S.Epoch,S.Sequence,S.PhysicalSeconds,S.RequestedGaugePressurePa.X,S.RequestedGaugePressurePa.Y,S.RequestedGaugePressurePa.Z,S.AppliedGaugePressurePa.X,S.AppliedGaugePressurePa.Y,S.AppliedGaugePressurePa.Z,
                S.PressureAppliedRevision,S.ProcessedControlRevision,S.AppliedSupplierMask,S.AppliedHopperOpen,S.AppliedReceiverOpen,S.AppliedRPM,S.WeighingMass.Kg,S.PipeMass.Kg,S.ReceiverMass.Kg,S.Transfer.Id,
                S.Transfer.BaselineKg,S.Transfer.RemainingKg,S.Transfer.InTransitKg,S.Transfer.ArrivedKg,S.Transfer.UnclassifiedKg,int32(S.Transfer.State),S.Transfer.CalculatedCarrierFlowM3s,S.WeighingRegulatorM3s,*Reason));
            LastLog=Now;
        }
        if((Step<21||Step>=30)&&Now-StageWall>45)return End(TEXT("CONTROL_TIMEOUT"),M.StopDescription());
        if(Now-Start>2400)return End(TEXT("LIMIT"),TEXT("Bounded observation wall limit reached; no complete-transfer claim."));
        if(S.Validity==EPlantValueValidity::Error)return End(TEXT("ERROR"),S.Error);
        if(S.Validity!=EPlantValueValidity::Valid||S.WeighingMass.Validity!=EPlantValueValidity::Valid)return true;
        if(!S.PressureControlEnabled)return End(TEXT("FAIL"),TEXT("Saved LowPoly actor did not enable original regulated-pressure setting."));
        auto Select=[&](const TCHAR* Id){auto* F=Widget->FindFunction(TEXT("SelectEquipment_0"));if(!F)return false;FStructOnScope P(F);auto* D=P.GetStructMemory();
            FindFProperty<FStrProperty>(F,TEXT("EquipmentId"))->SetPropertyValue_InContainer(D,Id);
            FString Name=FString(Id)==TEXT("weigher")?TEXT("칭량호퍼"):FString(Id)==TEXT("receiver")?TEXT("수취호퍼"):FString(Id)==TEXT("mixer")?TEXT("혼합기"):FString(Id);
            for(const TCHAR* N:{TEXT("EquipmentLabelInput"),TEXT("EquipmentDescriptionInput")}){
                if(auto* T=FindFProperty<FTextProperty>(F,N))T->SetPropertyValue_InContainer(D,FText::FromString(Name));if(auto* T=FindFProperty<FStrProperty>(F,N))T->SetPropertyValue_InContainer(D,Name);}
            FindFProperty<FBoolProperty>(F,TEXT("IsMixer"))->SetPropertyValue_InContainer(D,FString(Id)==TEXT("mixer"));Widget->ProcessEvent(F,D);UI->Adapter->Refresh();UI->ControlUI->Refresh();return true;};
        auto Click=[&](const TCHAR* Name){auto* B=Cast<UButton>(Widget->GetWidgetFromName(Name));if(!B||!B->GetIsEnabled())return false;B->OnClicked.Broadcast();LastRequest=M.LastRequest;return true;};
        auto Set=[&](const TCHAR* Name,const TCHAR* Value){auto* I=Cast<UEditableTextBox>(Widget->GetWidgetFromName(Name));if(!I)return false;I->SetText(FText::FromString(Value));return true;};
        auto Pressure=[&](const TCHAR* Id,const TCHAR* KPa){return Select(Id)&&Set(TEXT("PressureInput"),KPa)&&Click(TEXT("ApplyPressure"));};
        auto Applied=[&](){auto* R=M.History.Find(LastRequest);return R&&R->Status==EProcessCommandStatus::Applied;};
        auto Event=[&](const FString& E){Append(TEXT("pie_events.csv"),FString::Printf(TEXT("%.3f,%.9g,%d,%s\n"),Now-Start,S.PhysicalSeconds,Step,*E));};
        bool Action=true;
        switch(Step)
        {
        case 0:
            if(Service->GetSource()->ParticleCount!=20000)return End(TEXT("FAIL"),TEXT("Unexpected original initial charge"));
            Event(TEXT("20000 original particles; saved regulated-pressure actor active"));Action=Select(TEXT("weigher"))&&Click(TEXT("GlobalStop"));break;
        case 1:if(M.StopStage!=EProcessStopStage::Complete)return true;Frozen=S.PhysicalSeconds;Mark=Now;break;
        case 2:
            if(Now-Mark<1)return true;if(S.PhysicalSeconds!=Frozen)return End(TEXT("FAIL"),TEXT("Initial stop did not freeze simulation time"));
            Action=Click(TEXT("ManualResume"));break;
        case 3:if(!Applied())return true;Action=Pressure(TEXT("supply-a"),TEXT("0.125"));break;
        case 4:
            if(!Applied())return true;
            if(S.AppliedGaugePressurePa.X!=125)return End(TEXT("FAIL"),TEXT("UI kPa/Pa conversion mismatch"));
            OldRequest=LastRequest;BeforeRevision=S.RequestedControlRevision;
            Service->Request(TEXT("supply-a"),EProcessCommand::Pressure,125,OldRequest);
            if(Service->GetSource()->GetSimulationSnapshot().RequestedControlRevision!=BeforeRevision)return End(TEXT("FAIL"),TEXT("Duplicate pressure transmitted twice"));
            Action=Select(TEXT("supply-f"));
            if(UI->Adapter->ReadEquipment(TEXT("supply-f")).AppliedPressurePa.Value!=125)return End(TEXT("FAIL"),TEXT("Supply common mapping mismatch"));
            BeforeHistory=M.History.Num();Action=Action&&Set(TEXT("PressureInput"),TEXT("0.75"));break;
        case 5:
            if(M.History.Num()!=BeforeHistory||S.RequestedControlRevision!=BeforeRevision)return End(TEXT("FAIL"),TEXT("Text edit or read caused pressure command"));
            Action=Click(TEXT("View3D"));Mark=Now;Capture(TEXT("protocol_3d.png"));break;
        case 6:
            if(M.History.Num()!=BeforeHistory||S.RequestedControlRevision!=BeforeRevision)return End(TEXT("FAIL"),TEXT("3D view caused command"));
            if(auto* P=FindFProperty<FBoolProperty>(Widget->GetClass(),TEXT("Is3D"));!P||!P->GetPropertyValue_InContainer(Widget))return End(TEXT("FAIL"),TEXT("3D view flag did not change"));
            if(Now-Mark<1)return true;Action=Click(TEXT("View2D"));break;
        case 7:
            if(M.History.Num()!=BeforeHistory||S.RequestedControlRevision!=BeforeRevision)return End(TEXT("FAIL"),TEXT("2D view caused command"));
            if(auto* P=FindFProperty<FBoolProperty>(Widget->GetClass(),TEXT("Is3D"));!P||P->GetPropertyValue_InContainer(Widget))return End(TEXT("FAIL"),TEXT("2D view flag did not change"));
            Action=Select(TEXT("supply-f"))&&Set(TEXT("PressureInput"),TEXT("1.001"))&&Click(TEXT("ApplyPressure"));break;
        case 8:
            if(M.History[LastRequest].Status!=EProcessCommandStatus::Rejected||S.RequestedControlRevision!=BeforeRevision)return End(TEXT("FAIL"),TEXT("Range input not rejected atomically"));
            Action=Set(TEXT("PressureInput"),TEXT("invalid"))&&Click(TEXT("ApplyPressure"));break;
        case 9:
            if(M.History[LastRequest].Status!=EProcessCommandStatus::Rejected||S.RequestedControlRevision!=BeforeRevision)return End(TEXT("FAIL"),TEXT("Invalid text not rejected"));
            Event(TEXT("125 Pa common mapping; duplicate; invalid/range; typing and 2D/3D no commands verified"));Action=Pressure(TEXT("weigher"),TEXT("-0.25"));break;
        case 10:if(!Applied())return true;Action=Pressure(TEXT("receiver"),TEXT("-0.5"));break;
        case 11:if(!Applied())return true;Action=Select(TEXT("mixer"))&&Set(TEXT("RPMInput"),TEXT("7"))&&Click(TEXT("SetRPM"));break;
        case 12:if(!Applied())return true;Action=Click(TEXT("MixerStart"));break;
        case 13:if(!Applied())return true;Capture(TEXT("mixer_pressure_running.png"));Action=Select(TEXT("weigher"))&&Click(TEXT("GlobalStop"));break;
        case 14:
            if(M.StopStage!=EProcessStopStage::Complete)return true;
            if(S.AppliedGaugePressurePa!=FVector::ZeroVector||S.RequestedGaugePressurePa!=FVector::ZeroVector||S.AppliedRPM!=0)return End(TEXT("FAIL"),TEXT("Stop completed without zero pressure/RPM"));
            if(auto* Input=Cast<UEditableTextBox>(Widget->GetWidgetFromName(TEXT("PressureInput")));!Input||Input->GetText().ToString()!=TEXT("0"))return End(TEXT("FAIL"),TEXT("Stop retained old pressure draft"));
            if(!StopShot){StopShot=true;Mark=Now;Capture(TEXT("pressure_stop.png"));return true;}if(Now-Mark<1)return true;
            Event(TEXT("all pressure zero; valves closed; RPM zero then worker pause confirmed"));Action=Click(TEXT("ManualResume"));break;
        case 15:
            if(!Applied())return true;
            if(S.RequestedGaugePressurePa!=FVector::ZeroVector||S.AppliedGaugePressurePa!=FVector::ZeroVector)return End(TEXT("FAIL"),TEXT("Old pressure restored on resume"));
            Event(TEXT("resume preserves zero; explicit new pressure required"));OldEpoch=S.Epoch;Service->GetSource()->ResetSimulation();break;
        case 16:
            if(S.Epoch<=OldEpoch)return true;
            if(S.RequestedGaugePressurePa!=FVector::ZeroVector||S.AppliedGaugePressurePa!=FVector::ZeroVector||S.Transfer.Id!=0)return End(TEXT("FAIL"),TEXT("Reset retained pressure/cohort"));
            if(Service->RequestForEpoch(FGuid::NewGuid(),OldEpoch,TEXT("weigher"),EProcessCommand::Pressure,500).Status!=EProcessCommandStatus::Rejected)return End(TEXT("FAIL"),TEXT("Old epoch accepted"));
            if(Service->Request(TEXT("supply-a"),EProcessCommand::Pressure,125,OldRequest).Status!=EProcessCommandStatus::Rejected)return End(TEXT("FAIL"),TEXT("Old pressure RequestId accepted"));
            Event(TEXT("reset zero; old epoch and RequestId rejected; original charge reloaded"));Action=Click(TEXT("GlobalStop"));break;
        case 17:
            if(M.StopStage!=EProcessStopStage::Complete)return true;
            if(FParse::Param(FCommandLine::Get(),TEXT("PressureUIProtocolOnly"))){Stopping=true;return End(TEXT("CONTROL_ONLY_PASS"),TEXT("Focused UI protocol regression including draft reset; no repeated transfer run."));}
            Action=Click(TEXT("ManualResume"));break;
        case 18:if(!Applied())return true;Action=Pressure(TEXT("supply-a"),TEXT("0.5"));break;
        case 19:if(!Applied())return true;Action=Pressure(TEXT("weigher"),TEXT("-0.5"));break;
        case 20:if(!Applied())return true;Action=Select(TEXT("supply-a"))&&Click(TEXT("ValveOpen"));Mark=S.PhysicalSeconds;break;
        case 21:
            if(S.WeighingMass.Kg<.0004){if(S.PhysicalSeconds-Mark>1.5)return End(TEXT("NO_FEED"),TEXT("No WH material within bounded feed observation"));return true;}
            Event(TEXT("WH mass increased under supply +500 WH -500 Pa; request manual A close"));Action=Click(TEXT("ValveClose"));break;
        case 22:
            if(S.AppliedSupplierMask||S.RequestedSuppliers[0]){if(S.PhysicalSeconds-Mark>3)return End(TEXT("GATE_PENDING"),TEXT("Supply gate still occupied; no repeated long run"));return true;}
            Event(TEXT("all supplier gates including F confirmed closed"));Action=Click(TEXT("GlobalStop"));break;
        case 23:
            if(M.StopStage==EProcessStopStage::Failed)return End(TEXT("STOP_FAILED"),M.StopDescription());
            if(M.StopStage!=EProcessStopStage::Complete)return true;Action=Click(TEXT("ManualResume"));break;
        case 24:if(!Applied())return true;Action=Pressure(TEXT("weigher"),TEXT("0.5"));break;
        case 25:if(!Applied())return true;Action=Pressure(TEXT("receiver"),TEXT("-1"));break;
        case 26:if(!Applied())return true;Mark=S.PhysicalSeconds;break;
        case 27:
            if(S.PhysicalSeconds-Mark<.2)return true;
            if(S.AppliedSupplierMask!=0||S.AppliedGaugePressurePa!=FVector(0,500,-1000))return End(TEXT("FAIL"),TEXT("Independent-transfer preconditions mismatch"));
            Event(TEXT("manual WH open; supply=0 WH=500 RH=-1000 Pa; all supply valves closed; F never opened"));Action=Select(TEXT("weigher"))&&Click(TEXT("ValveOpen"));break;
        case 28:
            if(!Applied()||!S.Transfer.Id)return true;Mark=S.PhysicalSeconds;Stable=Mark;LastTransit=S.Transfer.InTransitKg;Capture(TEXT("transfer_started.png"));break;
        case 29:
            if(S.AppliedSupplierMask!=0)return End(TEXT("FAIL"),TEXT("Unexpected supplier opening during independent transfer"));
            if(S.Transfer.InTransitKg>0)SawPipe=true;if(S.Transfer.ArrivedKg>0)SawArrival=true;
            if(S.Transfer.State==EPlantTransferState::Complete&&(S.Transfer.InTransitKg>S.Transfer.Criteria.PipeResidualKg||S.Transfer.RemainingKg>S.Transfer.Criteria.WeighingResidualKg))return End(TEXT("FAIL"),TEXT("Completion with excessive residue"));
            if(S.Transfer.InTransitKg!=LastTransit){LastTransit=S.Transfer.InTransitKg;Stable=S.PhysicalSeconds;}
            if(S.Transfer.State!=EPlantTransferState::Complete&&S.PhysicalSeconds-Mark<3&&!(S.PhysicalSeconds-Mark>=1.5&&S.PhysicalSeconds-Stable>=.5))return true;
            Event(TEXT("bounded transfer observation ended; completion only if actual cohort criteria met"));Capture(TEXT("transfer_observed.png"));
            Action=Click(TEXT("ManualPause"));Stopping=true;break;
        case 30:
            if(M.StopStage==EProcessStopStage::Failed){if(M.FreezeState==EProcessFreezeState::Awaiting)return true;return End(TEXT("STOP_FAILED"),M.StopDescription()+TEXT(" ")+M.StopFailureEquipment);}
            if(M.StopStage!=EProcessStopStage::Complete)return true;Frozen=S.PhysicalSeconds;Mark=Now;break;
        case 31:
            if(Now-Mark<1)return true;if(S.PhysicalSeconds!=Frozen)return End(TEXT("FAIL"),TEXT("Final pause advanced time"));
            Event(TEXT("final pressure zero/closed/RPM zero then worker pause confirmed"));Action=Click(TEXT("ManualResume"));break;
        case 32:
            if(!Applied())return true;if(S.AppliedGaugePressurePa!=FVector::ZeroVector||S.RequestedGaugePressurePa!=FVector::ZeroVector)return End(TEXT("FAIL"),TEXT("Final resume restored pressure"));
            Action=Click(TEXT("GlobalStop"));break;
        default:
            if(M.StopStage!=EProcessStopStage::Complete)return true;
            return End(S.Transfer.State==EPlantTransferState::Complete?TEXT("COMPLETE"):TEXT("CONTROL_PASS_TRANSFER_LIMITED"),TEXT("Manual pressure UI and stop/resume verified. Transfer outcome and late inflow remain as observed; no tolerance/physics changes."));
        }
        if(!Action)return End(TEXT("FAIL"),TEXT("Expected existing UMG control unavailable"));
        Event(TEXT("step passed"));StageWall=Now;++Step;return true;
    }),.25f);
}
struct FAutoPressurePIE
{
    FAutoPressurePIE(){if(!FParse::Param(FCommandLine::Get(),TEXT("PressureUIValidation")))return;
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float){
            if(!GEngine)return true;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE&&C.World()&&
                UWorld::RemovePIEPrefix(C.World()->GetOutermost()->GetName())==ProcessPlantBinding::LowPolyMap)
            {auto* UI=C.World()->GetSubsystem<UProcessUIStateSubsystem>();if(UI&&UI->ControlService&&UI->StatusWidget){PressurePIE(C.World());return false;}}
            return true;}),.5f);
    }
} AutoPressurePIE;
}
#endif
