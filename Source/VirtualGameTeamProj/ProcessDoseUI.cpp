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
namespace {
double Grams(UUserWidget* W,const TCHAR* Name){
 auto* I=Cast<UEditableTextBox>(W->GetWidgetFromName(Name));if(!I)return std::numeric_limits<double>::quiet_NaN();
 const FString T=I->GetText().ToString().TrimStartAndEnd();FTCHARToUTF8 Bytes(*T);char* End=nullptr;const double G=std::strtod(Bytes.Get(),&End);
 return T.IsEmpty()||End==Bytes.Get()||!End||*End?std::numeric_limits<double>::quiet_NaN():G/1000.;
}
}
void UProcessControlUI::BuildDoseControls(UVerticalBox* Panel)
{
 auto* W=Widget.Get();if(!W||!Panel)return;
 DoseControls=W->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("AutoDoseControls"));
 Panel->AddChildToVerticalBox(DoseControls)->SetPadding(FMargin(6,6));
 auto Text=[&](const TCHAR* Name,const TCHAR* Value){auto* T=W->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);
  auto F=T->GetFont();F.Size=15;T->SetFont(F);T->SetAutoWrapText(true);T->SetColorAndOpacity(FLinearColor(.025f,.055f,.09f));T->SetText(FText::FromString(Value));return T;};
 DoseControls->AddChildToVerticalBox(Text(TEXT("DoseTitle"),TEXT("자동 배치 · A 단독 / A·B 순차 계량")));
 auto* Row=W->WidgetTree->ConstructWidget<UHorizontalBox>();DoseControls->AddChildToVerticalBox(Row)->SetPadding(FMargin(0,4));
 auto Button=[&](const TCHAR* Name,const TCHAR* Value){auto* B=W->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),Name);
  auto* T=Text(TEXT(""),Value);T->SetAutoWrapText(false);B->AddChild(T);
  auto BS=B->GetStyle();BS.Normal=FSlateRoundedBoxBrush(FLinearColor(.76f,.88f,.94f),6.f);BS.Hovered=FSlateRoundedBoxBrush(FLinearColor(.65f,.83f,.92f),6.f);
  BS.Pressed=FSlateRoundedBoxBrush(FLinearColor(.54f,.74f,.86f),6.f);BS.Disabled=FSlateRoundedBoxBrush(FLinearColor(.88f,.90f,.92f),6.f);BS.NormalPadding=BS.PressedPadding=FMargin(8,7);B->SetStyle(BS);return B;};
 auto* Full=Button(TEXT("AutoModeFull"),TEXT("전량"));Row->AddChildToHorizontalBox(Full)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Full->OnClicked.AddDynamic(this,&UProcessControlUI::SelectFullBatch);
 auto* Target=Button(TEXT("AutoModeTarget"),TEXT("목표량"));Row->AddChildToHorizontalBox(Target)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Target->OnClicked.AddDynamic(this,&UProcessControlUI::SelectTargetBatch);
 auto* AB=Button(TEXT("AutoModeSequential"),TEXT("A·B 순차"));Row->AddChildToHorizontalBox(AB)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));AB->OnClicked.AddDynamic(this,&UProcessControlUI::SelectSequentialBatch);
 auto Input=[&](const TCHAR* Name,const TCHAR* Hint){auto* I=W->WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(),Name);
  auto Style=I->GetWidgetStyle();auto Font=Style.TextStyle.Font;Font.Size=17;Style.SetFont(Font);Style.SetForegroundColor(FLinearColor(.025f,.055f,.09f));I->SetWidgetStyle(Style);
  I->SetHintText(FText::FromString(Hint));DoseControls->AddChildToVerticalBox(I)->SetPadding(FMargin(0,3));};
 DoseControls->AddChildToVerticalBox(Text(TEXT("TargetMassLabel"),TEXT("A 목표량 (g)")));Input(TEXT("TargetMassInput"),TEXT("g 단위 양수"));
 DoseControls->AddChildToVerticalBox(Text(TEXT("WeighToleranceLabel"),TEXT("A 계량 허용 오차 ± (g)")));Input(TEXT("WeighToleranceInput"),TEXT("g 단위 0 이상, 목표량 미만"));
 auto* Apply=Button(TEXT("ApplyTargetMass"),TEXT("목표량·계량 오차 적용"));DoseControls->AddChildToVerticalBox(Apply)->SetPadding(FMargin(0,3));Apply->OnClicked.AddDynamic(this,&UProcessControlUI::ApplyTargetMass);
 DoseControls->AddChildToVerticalBox(Text(TEXT("BTargetMassLabel"),TEXT("B 목표량 (g) · A·B 순차 모드")));Input(TEXT("BTargetMassInput"),TEXT("B g 단위 양수"));
 DoseControls->AddChildToVerticalBox(Text(TEXT("BWeighToleranceLabel"),TEXT("B 계량 허용 오차 ± (g)")));Input(TEXT("BWeighToleranceInput"),TEXT("B g 단위 0 이상"));
 auto* ApplyB=Button(TEXT("ApplyBTargetMass"),TEXT("B 목표량·계량 오차 적용"));DoseControls->AddChildToVerticalBox(ApplyB)->SetPadding(FMargin(0,3));ApplyB->OnClicked.AddDynamic(this,&UProcessControlUI::ApplyBTargetMass);
 DoseControls->AddChildToVerticalBox(Text(TEXT("AutoDoseSummary"),TEXT("")));
 BuildAdditiveControls(DoseControls);
 // No edit/Enter/refresh callbacks dispatch controls. Explicit apply only changes settings.
}
void UProcessControlUI::SelectFullBatch(){if(Is3D())return;if(auto* S=Service.Get()){FString Why;DoseMessage=S->SetAutomaticBatchMode(EProcessBatchMode::Full,Why)?TEXT("전량 모드 선택"):Why;Refresh();}}
void UProcessControlUI::SelectTargetBatch(){if(Is3D())return;if(auto* S=Service.Get()){FString Why;DoseMessage=S->SetAutomaticBatchMode(EProcessBatchMode::TargetMass,Why)?TEXT("목표량 시험 모드 · 공급 중 A 실제 닫힘은 확인이 필요합니다. 허용 범위 확정 전 출구 개방을 차단합니다."):Why;Refresh();}}
void UProcessControlUI::ApplyTargetMass()
{
 if(Is3D())return;auto* W=Widget.Get();auto* S=Service.Get();if(!W||!S)return;
 FString Why;DoseMessage=S->SetAutomaticTargetMass(Grams(W,TEXT("TargetMassInput")),Grams(W,TEXT("WeighToleranceInput")),Why)?TEXT("목표량·계량 오차 적용 완료 · 시작 시 이번 배치 값으로 확정"):Why;Refresh();
}
void UProcessControlUI::RefreshDoseControls()
{
 auto* S=Service.Get();auto* W=Widget.Get();if(!S||!W||!DoseControls)return;
 const bool Show=!Is3D()&&(!OperatorLayoutBuilt||IndividualSettingsOpen)&&(OperatorLayoutBuilt||Selected()==TEXT("supply-a")||Selected()==TEXT("supply-b")||Selected()==TEXT("weigher"))&&S->State.Snapshot.SupplyABatch.Supported;
 DoseControls->SetVisibility(Show?ESlateVisibility::Visible:ESlateVisibility::Collapsed);if(!Show)return;
 RefreshAdditiveControls();
 const bool Editable=S->State.Connected&&!S->AutoRun.State.Active&&!S->State.IsStopping();
 for(const TCHAR* N:{TEXT("AutoModeSequential"),TEXT("BTargetMassInput"),TEXT("BWeighToleranceInput"),TEXT("ApplyBTargetMass"),TEXT("AutoModeFull"),TEXT("AutoModeTarget"),TEXT("TargetMassInput"),TEXT("WeighToleranceInput"),TEXT("ApplyTargetMass")})if(auto* C=W->GetWidgetFromName(N))C->SetIsEnabled(Editable);
 const auto& V=S->AutoSettings;const auto& Snap=S->State.Snapshot;
 if(auto* T=Cast<UTextBlock>(W->GetWidgetFromName(TEXT("AutoDoseSummary"))))T->SetText(FText::FromString(
  FString::Printf(TEXT("선택: %s · 적용 목표 %.6f g ± %.6f g\n입자당 %.9f g · 현재 A 상류 %.6f g\n계량 오차와 이송 질량 보존 오차(%.6f g)는 별개입니다.\n%s"),
   V.BatchMode==EProcessBatchMode::Full?TEXT("A 전량"):(V.BatchMode==EProcessBatchMode::SequentialAB?TEXT("A·B 순차 계량"):TEXT("A 목표량")),V.TargetMassKg*1000,V.WeighingToleranceKg*1000,Snap.SourceKg(0)*1000,Snap.SequentialBatch.A.SourceCount*Snap.SourceKg(0)*1000,Snap.Transfer.Criteria.MassErrorKg*1000,*DoseMessage)+FString::Printf(TEXT("\nB 적용 목표 %.6f g ± %.6f g · B 상류 %.6f g"),V.TargetMassBKg*1000,V.WeighingToleranceBKg*1000,Snap.SequentialBatch.B.SourceCount*Snap.SourceKg(1)*1000)));
}

void UProcessControlUI::SelectSequentialBatch(){if(Is3D())return;if(auto* S=Service.Get()){FString Why;DoseMessage=S->SetAutomaticBatchMode(EProcessBatchMode::SequentialAB,Why)?TEXT("A 다음 B 계량 · 각각 허용 범위 확인 후 함께 혼합"):Why;Refresh();}}
void UProcessControlUI::ApplyBTargetMass(){if(Is3D())return;auto* W=Widget.Get();auto* S=Service.Get();if(!W||!S)return;FString Why;DoseMessage=S->SetAutomaticBTargetMass(Grams(W,TEXT("BTargetMassInput")),Grams(W,TEXT("BWeighToleranceInput")),Why)?TEXT("B 목표량·계량 오차 적용 완료"):Why;Refresh();}
