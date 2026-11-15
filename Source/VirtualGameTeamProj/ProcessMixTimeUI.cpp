#include "ProcessControlUI.h"
#include "ProcessControlService.h"
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
#include <cstdlib>
#include <limits>

void UProcessControlUI::BuildMixTimeControls(UVerticalBox* Panel)
{
 auto* W=Widget.Get();if(!W||!Panel)return;
 MixTimeControls=W->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("AutoMixTimeControls"));
 Panel->AddChildToVerticalBox(MixTimeControls)->SetPadding(FMargin(6,6));
 auto Text=[&](const TCHAR* Name){auto* T=W->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);
  auto Font=T->GetFont();Font.Size=15;T->SetFont(Font);T->SetAutoWrapText(true);T->SetColorAndOpacity(FLinearColor(.025f,.055f,.09f));return T;};
 auto* Title=Text(TEXT("AutoMixTimeLabel"));Title->SetText(FText::FromString(TEXT("A 자동 배치 · 혼합 시간 (초)")));MixTimeControls->AddChildToVerticalBox(Title);
 auto* Row=W->WidgetTree->ConstructWidget<UHorizontalBox>();MixTimeControls->AddChildToVerticalBox(Row)->SetPadding(FMargin(0,4));
 auto* Input=W->WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(),TEXT("AutoMixTimeInput"));
 auto Style=Input->GetWidgetStyle();auto Font=Style.TextStyle.Font;Font.Size=17;Style.SetFont(Font);Style.SetForegroundColor(FLinearColor(.025f,.055f,.09f));Input->SetWidgetStyle(Style);
 Input->SetHintText(FText::FromString(TEXT("초 단위로 직접 입력")));Row->AddChildToHorizontalBox(Input)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
 auto* Apply=W->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("ApplyAutomaticMixTime"));
 auto* Caption=Text(TEXT("AutoMixTimeApplyText"));Caption->SetAutoWrapText(false);Caption->SetText(FText::FromString(TEXT("시간 적용")));Apply->AddChild(Caption);
 auto BS=Apply->GetStyle();BS.Normal=FSlateRoundedBoxBrush(FLinearColor(.76f,.88f,.94f),6.f);BS.Hovered=FSlateRoundedBoxBrush(FLinearColor(.65f,.83f,.92f),6.f);
 BS.Pressed=FSlateRoundedBoxBrush(FLinearColor(.54f,.74f,.86f),6.f);BS.Disabled=FSlateRoundedBoxBrush(FLinearColor(.88f,.90f,.92f),6.f);BS.NormalPadding=BS.PressedPadding=FMargin(8,7);Apply->SetStyle(BS);
 Row->AddChildToHorizontalBox(Apply)->SetPadding(FMargin(8,0));Apply->OnClicked.AddDynamic(this,&UProcessControlUI::ApplyAutomaticMixTime);
 MixTimeControls->AddChildToVerticalBox(Text(TEXT("AutoMixTimeSummary")));
 auto* Help=Text(TEXT("AutoMixTimeHelp"));Help->SetText(FText::FromString(TEXT("이번 배치 값은 시작할 때 확정합니다. 실제 RPM 적용부터 해석 시간으로 계산하며, 정지 적용 확인 후 배치 완료로 표시합니다. 혼합 균일도 판정은 포함하지 않습니다.")));MixTimeControls->AddChildToVerticalBox(Help);
 // Editing, Enter, refreshing, and changing 2D/3D do not apply settings or send commands.
}
void UProcessControlUI::ApplyAutomaticMixTime()
{
 if(Is3D())return;auto* W=Widget.Get();auto* S=Service.Get();if(!W||!S)return;
 auto* Input=Cast<UEditableTextBox>(W->GetWidgetFromName(TEXT("AutoMixTimeInput")));
 double Value=std::numeric_limits<double>::quiet_NaN();
 if(Input){const FString T=Input->GetText().ToString().TrimStartAndEnd();FTCHARToUTF8 Bytes(*T);char* End=nullptr;
  Value=std::strtod(Bytes.Get(),&End);if(T.IsEmpty()||End==Bytes.Get()||!End||*End)Value=std::numeric_limits<double>::quiet_NaN();}
 FString Reason;MixTimeMessage=S->SetAutomaticMixingDuration(Value,Reason)?TEXT("혼합 시간 적용 완료 · 시작 시 이번 배치 값으로 확정"):Reason;
 Refresh();
}
void UProcessControlUI::RefreshMixTimeControls()
{
 auto* S=Service.Get();auto* W=Widget.Get();if(!S||!W||!MixTimeControls)return;
 const bool Show=!Is3D()&&Selected()==TEXT("mixer")&&S->State.Snapshot.SupplyABatch.Supported;
 MixTimeControls->SetVisibility(Show?ESlateVisibility::Visible:ESlateVisibility::Collapsed);if(!Show)return;
 const bool Editable=S->State.Connected&&!S->AutoRun.State.Active&&!S->State.IsStopping();
 for(const TCHAR* N:{TEXT("AutoMixTimeInput"),TEXT("ApplyAutomaticMixTime")})if(auto* C=W->GetWidgetFromName(N))C->SetIsEnabled(Editable);
 if(auto* T=Cast<UTextBlock>(W->GetWidgetFromName(TEXT("AutoMixTimeSummary")))){
  const double Duration=S->AutoSettings.MixingDurationSeconds;
  const FString Applied=Duration>0?FString::Printf(TEXT("적용 혼합 시간 %.6g초"),Duration):TEXT("혼합 시간 미설정 · 운전 기본값 없음");
  T->SetText(FText::FromString(Applied+TEXT("\n")+MixTimeMessage));
 }
}
