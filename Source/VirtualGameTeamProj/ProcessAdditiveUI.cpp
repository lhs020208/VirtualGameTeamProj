#include "ProcessControlUI.h"
#include "ProcessControlService.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include <cstdlib>
#include <limits>
namespace {
double InputNumber(UUserWidget* W,const TCHAR* Name){
 auto* I=Cast<UEditableTextBox>(W->GetWidgetFromName(Name));if(!I)return std::numeric_limits<double>::quiet_NaN();
 const FString T=I->GetText().ToString().TrimStartAndEnd();FTCHARToUTF8 B(*T);char* End=nullptr;const double V=std::strtod(B.Get(),&End);
 return T.IsEmpty()||End==B.Get()||!End||*End?std::numeric_limits<double>::quiet_NaN():V;
}
}
void UProcessControlUI::BuildAdditiveControls(UVerticalBox* Panel)
{
 auto* W=Widget.Get();if(!W||!Panel)return;
 AdditiveControls=W->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("AdditiveControls"));
 Panel->AddChildToVerticalBox(AdditiveControls)->SetPadding(FMargin(0,10));
 auto Text=[&](const TCHAR* N,const TCHAR* Value){auto* T=W->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),N);auto F=T->GetFont();F.Size=15;T->SetFont(F);T->SetAutoWrapText(true);T->SetColorAndOpacity(FLinearColor(.025f,.055f,.09f));T->SetText(FText::FromString(Value));return T;};
 AdditiveControls->AddChildToVerticalBox(Text(TEXT("AdditiveTitle"),TEXT("A·B 배치 첨가제 1종 · 공용 계량 경로")));
 auto Check=[&](const TCHAR* N,const TCHAR* Label){auto* C=W->WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(),N);C->AddChild(Text(TEXT(""),Label));AdditiveControls->AddChildToVerticalBox(C)->SetPadding(FMargin(0,4));};
 Check(TEXT("AdditiveEnabledInput"),TEXT("첨가제 사용"));
 auto* Source=W->WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(),TEXT("AdditiveSupplierInput"));
 for(const TCHAR* Value:{TEXT("C"),TEXT("D"),TEXT("E"),TEXT("F")})Source->AddOption(Value);
 Source->SetSelectedOption(TEXT("C"));AdditiveControls->AddChildToVerticalBox(Source)->SetPadding(FMargin(0,4));
 auto Input=[&](const TCHAR* N,const TCHAR* Label){AdditiveControls->AddChildToVerticalBox(Text(TEXT(""),Label));auto* I=W->WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(),N);auto S=I->GetWidgetStyle();auto Font=S.TextStyle.Font;Font.Size=17;S.SetFont(Font);S.SetForegroundColor(FLinearColor(.025f,.055f,.09f));I->SetWidgetStyle(S);AdditiveControls->AddChildToVerticalBox(I)->SetPadding(FMargin(0,3));};
 Input(TEXT("AdditiveTargetInput"),TEXT("첨가제 목표량 (g)"));Input(TEXT("AdditiveToleranceInput"),TEXT("첨가제 계량 오차 ± (g)"));
 Check(TEXT("AdditiveScheduledInput"),TEXT("예약 ON · OFF는 혼합 전 초기 투입"));
 Input(TEXT("AdditiveDelayInput"),TEXT("혼합 시작 후 수취 출구를 열 시각 N (해석초) · ON 0초는 사전 준비 후 혼합 시작 직후 투입"));
 auto* B=W->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("ApplyAdditiveSettings"));B->AddChild(Text(TEXT(""),TEXT("첨가제 설정 적용")));
 auto Style=B->GetStyle();Style.Normal=FSlateRoundedBoxBrush(FLinearColor(.76f,.88f,.94f),6.f);Style.NormalPadding=Style.PressedPadding=FMargin(8,7);B->SetStyle(Style);
 B->OnClicked.AddDynamic(this,&UProcessControlUI::ApplyAdditiveSettings);AdditiveControls->AddChildToVerticalBox(B)->SetPadding(FMargin(0,4));
 AdditiveControls->AddChildToVerticalBox(Text(TEXT("AdditiveSummary"),TEXT("첨가제 미사용 · 시작 시 적용 설정 확정")));
}
void UProcessControlUI::ApplyAdditiveSettings()
{
 if(Is3D())return;auto* W=Widget.Get();auto* S=Service.Get();if(!W||!S)return;
 auto* E=Cast<UCheckBox>(W->GetWidgetFromName(TEXT("AdditiveEnabledInput")));auto* Scheduled=Cast<UCheckBox>(W->GetWidgetFromName(TEXT("AdditiveScheduledInput")));
 auto* Source=Cast<UComboBoxString>(W->GetWidgetFromName(TEXT("AdditiveSupplierInput")));if(!E||!Scheduled||!Source)return;
 FString Why;const int32 Supplier=Source->GetSelectedIndex()+2;
 AdditiveMessage=S->SetAutomaticAdditive(E->IsChecked(),Supplier,Scheduled->IsChecked(),InputNumber(W,TEXT("AdditiveTargetInput"))/1000.,InputNumber(W,TEXT("AdditiveToleranceInput"))/1000.,Scheduled->IsChecked()?InputNumber(W,TEXT("AdditiveDelayInput")):0.,Why)?TEXT("첨가제 설정 적용 · 시작 시 이번 배치 값으로 확정"):Why;
 Refresh();
}
void UProcessControlUI::RefreshAdditiveControls()
{
 auto* W=Widget.Get();auto* S=Service.Get();if(!W||!S||!AdditiveControls)return;
 const bool Editable=!Is3D()&&S->State.Connected&&!S->AutoRun.State.Active&&!S->State.IsStopping();
 for(const TCHAR* N:{TEXT("AdditiveEnabledInput"),TEXT("AdditiveSupplierInput"),TEXT("AdditiveTargetInput"),TEXT("AdditiveToleranceInput"),TEXT("AdditiveScheduledInput"),TEXT("AdditiveDelayInput"),TEXT("ApplyAdditiveSettings")})if(auto* C=W->GetWidgetFromName(N))C->SetIsEnabled(Editable);
 const auto& V=S->AutoSettings;const auto& Snapshot=S->State.Snapshot;
 if(auto* T=Cast<UTextBlock>(W->GetWidgetFromName(TEXT("AdditiveSummary"))))T->SetText(FText::FromString(
  FString::Printf(TEXT("적용: %s · %c %.6f g ± %.6f g · %s N=%.6f초\n현재 첨가제 상류 %s · 입자당 %.9f g\n예약 시각에 준비 미완료면 출구 닫힘·전체 정지. 혼합 시간 자동 연장 없음.\n%s"),
   V.AdditiveEnabled?TEXT("첨가제 사용"):TEXT("첨가제 미사용"),TCHAR('A'+V.AdditiveSupplier),V.AdditiveTargetKg*1000,V.AdditiveToleranceKg*1000,V.AdditiveScheduled?TEXT("예약 ON"):TEXT("예약 OFF"),V.AdditiveDelaySeconds,
   Snapshot.Recovery.Valid&&Snapshot.Recovery.Materials.IsValidIndex(V.AdditiveSupplier)&&Snapshot.Validity==EPlantValueValidity::Valid?*FString::Printf(TEXT("%g g"),Snapshot.Recovery.Materials[V.AdditiveSupplier].Source*Snapshot.SourceKg(V.AdditiveSupplier)*1000):TEXT("미확인"),Snapshot.SourceKg(V.AdditiveSupplier)*1000,*AdditiveMessage)));
}
