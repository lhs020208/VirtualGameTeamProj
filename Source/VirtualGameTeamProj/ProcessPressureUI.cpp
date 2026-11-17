#include "ProcessControlUI.h"
#include "ProcessControlService.h"
#include "ProcessDisplay.h"
#include "ProcessPlantBinding.h"
#include "ProcessUIStateAdapter.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "HAL/PlatformTime.h"
#include <cstdlib>
#include <limits>

void UProcessControlUI::BuildPressureControls(UVerticalBox* Panel)
{
    auto* W=Widget.Get();if(!W||!Panel)return;
    PressureControls=W->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("PressureControls"));
    Panel->AddChildToVerticalBox(PressureControls)->SetPadding(FMargin(6,6));
    auto Text=[&](const TCHAR* Name){
        auto* T=W->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);
        auto Font=T->GetFont();Font.Size=15;T->SetFont(Font);T->SetColorAndOpacity(FLinearColor(.025f,.055f,.09f));T->SetAutoWrapText(true);
        return T;
    };
    PressureControls->AddChildToVerticalBox(Text(TEXT("SupplyValvePressureSummary")))->SetPadding(FMargin(0,0,0,12));
    PressureControls->AddChildToVerticalBox(Text(TEXT("PressureLabel")));
    auto* Row=W->WidgetTree->ConstructWidget<UHorizontalBox>();PressureControls->AddChildToVerticalBox(Row)->SetPadding(FMargin(0,4));
    auto* Input=W->WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(),TEXT("PressureInput"));
    auto InputStyle=Input->GetWidgetStyle();auto InputFont=InputStyle.TextStyle.Font;InputFont.Size=17;
    InputStyle.SetFont(InputFont);InputStyle.SetForegroundColor(FLinearColor(.025f,.055f,.09f));Input->SetWidgetStyle(InputStyle);
    Input->SetHintText(FText::FromString(TEXT("목표 kPa(g)")));Row->AddChildToHorizontalBox(Input)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* Apply=W->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("ApplyPressure"));
    auto* Caption=Text(TEXT("ApplyPressureText"));Caption->SetAutoWrapText(false);Caption->SetText(FText::FromString(TEXT("압력 적용")));Apply->AddChild(Caption);
    auto ButtonStyle=Apply->GetStyle();ButtonStyle.Normal=FSlateRoundedBoxBrush(FLinearColor(.76f,.88f,.94f),6.f);
    ButtonStyle.Hovered=FSlateRoundedBoxBrush(FLinearColor(.65f,.83f,.92f),6.f);ButtonStyle.Pressed=FSlateRoundedBoxBrush(FLinearColor(.54f,.74f,.86f),6.f);
    ButtonStyle.Disabled=FSlateRoundedBoxBrush(FLinearColor(.88f,.90f,.92f),6.f);ButtonStyle.NormalPadding=ButtonStyle.PressedPadding=FMargin(8,7);Apply->SetStyle(ButtonStyle);
    Row->AddChildToHorizontalBox(Apply)->SetPadding(FMargin(8,0));Apply->OnClicked.AddDynamic(this,&UProcessControlUI::ApplyPressure);
    // Text edits, Enter, selection, refresh and 2D/3D changes have no command delegate.
    PressureControls->AddChildToVerticalBox(Text(TEXT("PressureAvailability")))->SetPadding(FMargin(0,4));
    PressureControls->AddChildToVerticalBox(Text(TEXT("TransferPressureSummary")));
    PressureControls->AddChildToVerticalBox(Text(TEXT("PressureSummary")));
    PressureControls->AddChildToVerticalBox(Text(TEXT("PressureHelp")));
    PressureControls->AddChildToVerticalBox(Text(TEXT("PressureCommandStatus")));
}

void UProcessControlUI::ApplyPressure()
{
    auto* W=Widget.Get();auto* Input=W?Cast<UEditableTextBox>(W->GetWidgetFromName(TEXT("PressureInput"))):nullptr;
    double KPa=std::numeric_limits<double>::quiet_NaN();
    if(Input){const FString Text=Input->GetText().ToString().TrimStartAndEnd();FTCHARToUTF8 Bytes(*Text);char* End=nullptr;
        KPa=std::strtod(Bytes.Get(),&End);if(Text.IsEmpty()||End==Bytes.Get()||!End||*End)KPa=std::numeric_limits<double>::quiet_NaN();}
    Send(EProcessCommand::Pressure,KPa*PlantControl::PaPerKPa);
}

void UProcessControlUI::RefreshPressureControls()
{
    auto* W=Widget.Get();auto* S=Service.Get();if(!W||!S||!PressureControls)return;
    const auto& M=S->State;const auto& Snapshot=M.Snapshot;const FString Id=Selected();
    const int32 Target=PlantControl::PressureTarget(Id);const double Now=FPlatformTime::Seconds();
    auto State=UProcessUIStateAdapter::BuildEquipment(Id,M.Connected?&Snapshot:nullptr,Now);
    if(!M.Connected)State.RequestedPressurePa.Validity=State.AppliedPressurePa.Validity=EPlantValueValidity::SourceMissing;
    const bool Show=Target!=INDEX_NONE&&Snapshot.PressureControlEnabled&&!Is3D();
    PressureControls->SetVisibility(Show?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    if(!Show)return;
    auto Label=[&](const TCHAR* Name,const FString& Value){if(auto* T=Cast<UTextBlock>(W->GetWidgetFromName(Name)))T->SetText(FText::FromString(Value));};
    auto Number=[](const FProcessStateValue& V){
        if(V.Validity==EPlantValueValidity::Valid&&FMath::IsFinite(V.Value))return ProcessDisplay::Number(V.Value/PlantControl::PaPerKPa)+TEXT(" kPa(g)");
        switch(V.Validity){case EPlantValueValidity::Stale:return FString(TEXT("갱신 중단"));case EPlantValueValidity::Error:return FString(TEXT("오류"));
        case EPlantValueValidity::SourceMissing:return FString(TEXT("미연결"));default:return FString(TEXT("확인 대기"));}
    };
    const int32 Supplier=ProcessPlantBinding::SupplierIndex(Id);
    if(auto* T=W->GetWidgetFromName(TEXT("SupplyValvePressureSummary")))T->SetVisibility(Supplier!=INDEX_NONE?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    if(Supplier!=INDEX_NONE){
        const bool Fresh=M.Connected&&Snapshot.Validity==EPlantValueValidity::Valid&&Snapshot.Error.IsEmpty()&&Now>=Snapshot.PublishedWallSeconds&&Now-Snapshot.PublishedWallSeconds<=2.;
        const auto Pa=[](double Value){return ProcessDisplay::Number(Value/1000.,3);};
        const bool Applied=Fresh&&Snapshot.AppliedPressureValid;
        FString Body=TEXT("공급호퍼 적용 압력 (A~F 공통)  ")+(Applied?Pa(Snapshot.AppliedGaugePressurePa.X)+TEXT(" kPa(g)"):TEXT("미확인"));
        const auto* V=Snapshot.SupplyValvePressures.IsValidIndex(Supplier)?&Snapshot.SupplyValvePressures[Supplier]:nullptr;
        if(Fresh&&V&&V->Valid&&(Snapshot.AppliedSupplierMask&(1<<Supplier))){
            Body+=TEXT("\n밸브 직전 압력  ")+Pa(V->BeforeGaugePa)+TEXT(" kPa(g)\n밸브 직후 압력  ")+Pa(V->AfterGaugePa)+TEXT(" kPa(g)\n밸브 앞뒤 구간 차압  ")+Pa(V->DifferencePa)+TEXT(" kPa");
            Body+=TEXT("\n모델 추정값 · 직전 − 직후 · 기존 구간 손실");
            Body+=FString::Printf(TEXT("\n위치: route %d / cell %d · z %.3f → %.3f m"),V->Route,V->GateCell,V->BeforeMetres.Z,V->AfterMetres.Z);
        }else Body+=TEXT("\n밸브 직전·직후 압력 / 밸브 앞뒤 구간 차압\n")+(Fresh&&V?V->Reason:FString(TEXT("최신 계산값 미확인 · 차압 미산출")));
        Body+=TEXT("\n칭량호퍼 적용 압력  ")+(Applied?Pa(Snapshot.AppliedGaugePressurePa.Y)+TEXT(" kPa(g)"):TEXT("미확인"));
        if(Applied)Body+=TEXT("\n호퍼 사이 전체 차압  ")+Pa(Snapshot.AppliedGaugePressurePa.X-Snapshot.AppliedGaugePressurePa.Y)+TEXT(" kPa (별도)");
        Body+=TEXT("\n게이지압 0 = 대기압 · 호퍼 값은 적용 압력 설정");
        Label(TEXT("SupplyValvePressureSummary"),Body);
    }
    Label(TEXT("PressureLabel"),State.PressureScope+TEXT(" · 목표 압력 kPa(g)"));
    Label(TEXT("PressureSummary"),TEXT("요청 설정  ")+Number(State.RequestedPressurePa)+TEXT("\n적용 압력 설정  ")+Number(State.AppliedPressurePa));
    Label(TEXT("PressureHelp"),FString(Target==2?TEXT("모델 시험 범위 -1~0 kPa(g). 출구 열림 시 적용 설정 0.\n"):TEXT("모델 시험 범위 -1~+1 kPa(g).\n"))+
        TEXT("0 = 대기압 (101.325 kPa 절대압). 1 kPa = 1,000 Pa.\n이상적인 조절기 경계조건 · 센서값/실제 감압 결과 아님.")+
        (Target==0?TEXT("\nA~F는 같은 설정을 공유합니다."):TEXT("")));
    auto* Input=Cast<UEditableTextBox>(W->GetWidgetFromName(TEXT("PressureInput")));
    if(PressureDraftEquipment!=Id||PressureDraftEpoch!=Snapshot.Epoch||PressureDraftConnection!=M.Connection||PressureDraftStop!=M.StopRequest)
    {
        // The service records StopRequest before the next snapshot reflects its atomic zero targets.
        // Clear the editor immediately rather than copying that pre-stop requested value back in.
        const bool NewStop=M.StopRequest.IsValid()&&PressureDraftStop!=M.StopRequest;
        PressureDraftEquipment=Id;PressureDraftEpoch=Snapshot.Epoch;PressureDraftConnection=M.Connection;PressureDraftStop=M.StopRequest;
        if(Input)Input->SetText(FText::FromString(NewStop?FString(TEXT("0")):State.RequestedPressurePa.Validity==EPlantValueValidity::Valid?
            ProcessDisplay::Exact(State.RequestedPressurePa.Value/PlantControl::PaPerKPa):FString()));
    }
    // Validate the submitted text on click, without silently clamping it or issuing a command here.
    const auto& Auto=S->AutoRun.State;
    const bool AutomaticReceiver=Target==2&&Auto.Active;
    const FString Reason=AutomaticReceiver?S->AutoRun.ReceiverPressureUnavailable(M,Now):M.UnavailableReason(Id,EProcessCommand::Pressure,0,Now);
    Label(TEXT("PressureAvailability"),Reason.IsEmpty()?(AutomaticReceiver?TEXT("현재 이송만 변경 · 다음 이송은 레시피 값 사용"):TEXT("새 목표값 입력 후 압력 적용")):Reason);
    if(auto* T=W->GetWidgetFromName(TEXT("TransferPressureSummary")))T->SetVisibility(Target==2?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    if(Target==2){
        const double RecipePa=Auto.Active?Auto.LockedTransferPressurePa.Z:S->AutoSettings.TransferPressurePa.Z;
        const bool Fresh=M.Connected&&Snapshot.Validity==EPlantValueValidity::Valid&&Snapshot.Error.IsEmpty()&&Now>=Snapshot.PublishedWallSeconds&&Now-Snapshot.PublishedWallSeconds<=2.;
        const bool Flow=Fresh&&Snapshot.AppliedHopperOpen&&!Snapshot.AppliedReceiverOpen&&Snapshot.Transfer.CarrierFlowValid;
        FString Body=TEXT("레시피 기준값  ")+ProcessDisplay::Number(RecipePa/1000.)+TEXT(" kPa(g)");
        if(Auto.Active&&Auto.Stage==EProcessAutoStage::Transferring&&Auto.ReceiverPressureRequest.IsValid()&&Auto.ReceiverPressureTransferId==Snapshot.Transfer.Id)
            Body+=TEXT("\n이번 이송 요청값  ")+ProcessDisplay::Number(Auto.ReceiverPressureRequestedPa/1000.)+TEXT(" kPa(g) · ")+Auto.ReceiverPressureStatus;
        Body+=TEXT("\n계산 이송 유량  ")+(Flow?ProcessDisplay::Number(Snapshot.Transfer.CalculatedCarrierFlowM3s,6)+TEXT(" m³/s"):TEXT("미산출 · 이송 유로/최신 계산 확인 필요"));
        Body+=TEXT("\n공급·칭량 압력은 자동 설정 유지");Label(TEXT("TransferPressureSummary"),Body);
    }
    if(Input)Input->SetIsEnabled(Reason.IsEmpty());
    if(auto* B=Cast<UButton>(W->GetWidgetFromName(TEXT("ApplyPressure")))){B->SetIsEnabled(Reason.IsEmpty());B->SetToolTipText(FText::FromString(Reason));}
    const FProcessCommandRecord* Latest=nullptr;
    for(const auto& P:M.History)if(P.Value.Connection==M.Connection&&P.Value.Epoch==Snapshot.Epoch&&
        ((P.Value.Command==EProcessCommand::Pressure&&PlantControl::SamePressureTarget(P.Value.EquipmentId,Id))||P.Value.UsesStopSequence)&&
        (!Latest||P.Value.AcceptedAt>Latest->AcceptedAt))Latest=&P.Value;
    Label(TEXT("PressureCommandStatus"),Latest?FProcessControlMachine::StatusText(Latest->Status)+TEXT(" · ")+Latest->Message:Reason);
    if(auto* T=W->GetWidgetFromName(TEXT("PressureCommandStatus")))T->SetToolTipText(FText::FromString(Latest?
        FString::Printf(TEXT("RequestId %s\n초기화 %lld · 연결 %lld · 요청 제어 번호 %lld\n작업자 압력 적용 번호 %lld"),
            *Latest->RequestId.ToString(),Latest->Epoch,Latest->Connection,Latest->Revision,Snapshot.PressureAppliedRevision):FString()));
}
