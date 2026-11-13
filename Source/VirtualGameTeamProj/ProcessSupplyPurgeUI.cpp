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

void UProcessControlUI::BuildPurgeControls(UVerticalBox* Panel)
{
 auto* W=Widget.Get();if(!W||!Panel)return;
 PurgeControls=W->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("PurgeControls"));
 Panel->AddChildToVerticalBox(PurgeControls)->SetPadding(FMargin(6,8));
 auto Text=[&](const TCHAR* Name,const TCHAR* Caption){auto* T=W->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);
  auto F=T->GetFont();F.Size=15;T->SetFont(F);T->SetColorAndOpacity(FLinearColor(.025f,.055f,.09f));T->SetAutoWrapText(true);T->SetText(FText::FromString(Caption));return T;};
 PurgeControls->AddChildToVerticalBox(Text(TEXT("PurgeLabel"),TEXT("공급 A 연결부 비우기용 공기 · 목표 kPa(g)")));
 auto Button=[&](const TCHAR* Name,const TCHAR* Caption){auto* B=W->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),Name);B->AddChild(Text(*(FString(Name)+TEXT("Text")),Caption));
  auto S=B->GetStyle();S.Normal=FSlateRoundedBoxBrush(FLinearColor(.76f,.88f,.94f),6.f);S.Hovered=FSlateRoundedBoxBrush(FLinearColor(.65f,.83f,.92f),6.f);S.Pressed=FSlateRoundedBoxBrush(FLinearColor(.54f,.74f,.86f),6.f);S.Disabled=FSlateRoundedBoxBrush(FLinearColor(.88f,.90f,.92f),6.f);S.NormalPadding=S.PressedPadding=FMargin(8,7);B->SetStyle(S);return B;};
 auto* Row=W->WidgetTree->ConstructWidget<UHorizontalBox>();PurgeControls->AddChildToVerticalBox(Row);
 auto* Input=W->WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(),TEXT("PurgePressureInput"));
 auto Style=Input->GetWidgetStyle();auto Font=Style.TextStyle.Font;Font.Size=17;Style.SetFont(Font);Style.SetForegroundColor(FLinearColor(.025f,.055f,.09f));Input->SetWidgetStyle(Style);Input->SetHintText(FText::FromString(TEXT("0~1 kPa(g)")));
 Row->AddChildToHorizontalBox(Input)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
 auto* Apply=Button(TEXT("PurgeApplyPressure"),TEXT("압력 적용"));Row->AddChildToHorizontalBox(Apply)->SetPadding(FMargin(8,0));Apply->OnClicked.AddDynamic(this,&UProcessControlUI::ApplyPurgePressure);
 auto* Actions=W->WidgetTree->ConstructWidget<UHorizontalBox>();PurgeControls->AddChildToVerticalBox(Actions)->SetPadding(FMargin(0,5));
 auto* Start=Button(TEXT("PurgeStart"),TEXT("연결부 공기 시작"));auto* Stop=Button(TEXT("PurgeStop"),TEXT("공기 정지·압력 0"));
 Actions->AddChildToHorizontalBox(Start)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Actions->AddChildToHorizontalBox(Stop)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
 Start->OnClicked.AddDynamic(this,&UProcessControlUI::StartPurge);Stop->OnClicked.AddDynamic(this,&UProcessControlUI::StopPurge);
 PurgeControls->AddChildToVerticalBox(Text(TEXT("PurgeSummary"),TEXT("확인 대기")));
 PurgeControls->AddChildToVerticalBox(Text(TEXT("PurgeHelp"),TEXT("A 밸브 하류 전용 · 공급 밸브와 칭량 출구 닫힘 필요. 모델 시험 범위 0~1 kPa(g), 0=대기압. 적용 압력은 경계 설정이며 센서값이 아닙니다. 칭량 압력보다 높게 설정하세요.")));
 PurgeControls->AddChildToVerticalBox(Text(TEXT("PurgeCommandStatus"),TEXT("")));
}
void UProcessControlUI::ApplyPurgePressure()
{
 auto* W=Widget.Get();auto* I=W?Cast<UEditableTextBox>(W->GetWidgetFromName(TEXT("PurgePressureInput"))):nullptr;
 double Pa=std::numeric_limits<double>::quiet_NaN();
 if(I){const FString T=I->GetText().ToString().TrimStartAndEnd();FTCHARToUTF8 B(*T);char* End=nullptr;Pa=std::strtod(B.Get(),&End)*PlantControl::PaPerKPa;if(T.IsEmpty()||End==B.Get()||!End||*End)Pa=std::numeric_limits<double>::quiet_NaN();}
 Send(EProcessCommand::PurgePressure,Pa);
}
void UProcessControlUI::StartPurge(){Send(EProcessCommand::PurgeEnabled,1);}
void UProcessControlUI::StopPurge(){Send(EProcessCommand::PurgeEnabled,0);}
void UProcessControlUI::RefreshPurgeControls()
{
 auto* W=Widget.Get();auto* S=Service.Get();if(!W||!S||!PurgeControls)return;
 const auto& M=S->State;const auto& Snap=M.Snapshot;const FString Id=Selected();const double Now=FPlatformTime::Seconds();
 const bool Show=Id==TEXT("supply-a")&&Snap.SupplyAPurge.Supported&&!Is3D();PurgeControls->SetVisibility(Show?ESlateVisibility::Visible:ESlateVisibility::Collapsed);if(!Show)return;
 const auto E=UProcessUIStateAdapter::BuildEquipment(Id,M.Connected?&Snap:nullptr,Now);const auto& P=E.SupplyAPurge;
 auto Label=[&](const TCHAR* N,const FString& T){if(auto* L=Cast<UTextBlock>(W->GetWidgetFromName(N)))L->SetText(FText::FromString(T));};
 FString Status;
 if(E.Validity==EPlantValueValidity::SourceMissing)Status=TEXT("미연결");
 else if(E.Validity==EPlantValueValidity::Stale)Status=TEXT("갱신 중단");
 else if(E.Validity==EPlantValueValidity::Error)Status=TEXT("작업자 오류");
 else if(!P.Valid)Status=TEXT("확인 대기");
 if(Status.IsEmpty())Label(TEXT("PurgeSummary"),FString::Printf(TEXT("요청 설정 %.6g / 적용 압력 설정 %.6g kPa(g)\n공기 요청 %s / 적용 %s · 계산 유량 %.6g m³/s\n공급 A 연결부 %d개 / %.6g g · 해석 %.4f초"),P.RequestedGaugePa/1000.,P.AppliedGaugePa/1000.,P.RequestedEnabled?TEXT("켜짐"):TEXT("꺼짐"),P.AppliedEnabled?TEXT("켜짐"):TEXT("꺼짐"),P.FlowM3s,P.ResidualCount,P.ResidualKg*1000.,P.ObservedSimulationSeconds));
 else Label(TEXT("PurgeSummary"),Status+TEXT(" · 이전 압력·잔량을 현재값으로 표시하지 않습니다."));
 auto* Input=Cast<UEditableTextBox>(W->GetWidgetFromName(TEXT("PurgePressureInput")));
 if(PurgeDraftEquipment!=Id||PurgeDraftEpoch!=Snap.Epoch||PurgeDraftConnection!=M.Connection||PurgeDraftStop!=M.StopRequest){
  const bool Stopping=M.StopRequest.IsValid()&&PurgeDraftStop!=M.StopRequest;PurgeDraftEquipment=Id;PurgeDraftEpoch=Snap.Epoch;PurgeDraftConnection=M.Connection;PurgeDraftStop=M.StopRequest;
  if(Input)Input->SetText(FText::FromString(Stopping?FString(TEXT("0")):P.Valid?FString::Printf(TEXT("%.9g"),P.RequestedGaugePa/1000.):FString()));
 }
 auto Update=[&](const TCHAR* Name,EProcessCommand C,double V){FString Why=M.UnavailableReason(Id,C,V,Now);if(auto* B=Cast<UButton>(W->GetWidgetFromName(Name))){B->SetIsEnabled(Why.IsEmpty());B->SetToolTipText(FText::FromString(Why));}return Why;};
 const auto Why=Update(TEXT("PurgeApplyPressure"),EProcessCommand::PurgePressure,Snap.SupplyAPurge.RequestedGaugePa);if(Input)Input->SetIsEnabled(Why.IsEmpty());
 Update(TEXT("PurgeStart"),EProcessCommand::PurgeEnabled,1);Update(TEXT("PurgeStop"),EProcessCommand::PurgeEnabled,0);
 const FProcessCommandRecord* Latest=nullptr;
 for(const auto& Pair:M.History){const auto& R=Pair.Value;if(R.Connection==M.Connection&&R.Epoch==Snap.Epoch&&(R.Command==EProcessCommand::PurgePressure||R.Command==EProcessCommand::PurgeEnabled||R.UsesStopSequence)&&(!Latest||R.AcceptedAt>Latest->AcceptedAt))Latest=&R;}
 Label(TEXT("PurgeCommandStatus"),!Status.IsEmpty()?Status:!P.Diagnostic.IsEmpty()?P.Diagnostic:Latest?FProcessControlMachine::StatusText(Latest->Status)+TEXT(" · ")+Latest->Message:Why);
 if(Latest)if(auto* T=W->GetWidgetFromName(TEXT("PurgeCommandStatus")))T->SetToolTipText(FText::FromString(FString::Printf(TEXT("RequestId %s · 초기화 %lld · 연결 %lld\n요청 %lld · 공기 작업자 적용 %lld"),*Latest->RequestId.ToString(),Latest->Epoch,Latest->Connection,Latest->Revision,P.AppliedRevision)));
}
