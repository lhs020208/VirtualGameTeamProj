#include "ProcessControlUI.h"
#include "ProcessControlService.h"
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
        if(V.Validity==EPlantValueValidity::Valid&&FMath::IsFinite(V.Value))return FString::Printf(TEXT("%.6g kPa(g)"),V.Value/PlantControl::PaPerKPa);
        switch(V.Validity){case EPlantValueValidity::Stale:return FString(TEXT("갱신 중단"));case EPlantValueValidity::Error:return FString(TEXT("오류"));
        case EPlantValueValidity::SourceMissing:return FString(TEXT("미연결"));default:return FString(TEXT("확인 대기"));}
    };
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
            FString::Printf(TEXT("%.9g"),State.RequestedPressurePa.Value/PlantControl::PaPerKPa):FString()));
    }
    // Validate the submitted text on click, without silently clamping it or issuing a command here.
    const FString Reason=M.UnavailableReason(Id,EProcessCommand::Pressure,0,Now);
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
