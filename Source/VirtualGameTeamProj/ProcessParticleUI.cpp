#include "ProcessControlUI.h"
#include "ProcessControlService.h"
#include "ProcessNumericField.h"
#include "ProcessDisplay.h"
#include "PowderPlantTest.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Brushes/SlateRoundedBoxBrush.h"

void UProcessControlUI::BuildParticleControls(UCanvasPanel* Root){
 auto* W=Widget.Get();auto* Tree=W->WidgetTree.Get();
 auto Text=[&](const FString& V,const TCHAR* N=TEXT("")){auto* T=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),N);auto F=T->GetFont();F.Size=22;T->SetFont(F);T->SetColorAndOpacity(FLinearColor(.035f,.075f,.12f));T->SetText(FText::FromString(V));T->SetAutoWrapText(true);return T;};
 ParticleSettingsPanel=Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("ParticleSettingsPanel"));ParticleSettingsPanel->SetPadding(FMargin(20));ParticleSettingsPanel->SetBrushColor(FLinearColor(.95f,.97f,.985f));
 auto* Slot=Root->AddChildToCanvas(ParticleSettingsPanel);Slot->SetAnchors(FAnchors(0,0,1,1));Slot->SetOffsets(FMargin(20,230,20,166));Slot->SetZOrder(60);
 auto* Scroll=Tree->ConstructWidget<UScrollBox>();ParticleSettingsPanel->SetContent(Scroll);auto* Box=Tree->ConstructWidget<UVerticalBox>();Scroll->AddChild(Box);
 Box->AddChildToVerticalBox(Text(TEXT("초기 입자 수 · 레시피와 별도")));
 Box->AddChildToVerticalBox(Text(TEXT("공급호퍼별 개수입니다. 적용하면 입자 위치·시간·밸브·혼합기 상태를 초기화합니다. 레시피는 유지됩니다. 전체 0개이면 입자 계산 없이 설비 동작만 1배속으로 실행합니다.")))->SetPadding(FMargin(0,8,0,16));
 auto* S=Service.Get();int32 Count=S&&S->GetSource()?S->GetSource()->GetInitialParticleCounts().Num():6;
 for(int32 I=0;I<Count;++I){auto* Row=Tree->ConstructWidget<UHorizontalBox>();Box->AddChildToVerticalBox(Row)->SetPadding(FMargin(0,6));Row->AddChildToHorizontalBox(Text(FString::Printf(TEXT("공급 %c  "),'A'+I)))->SetVerticalAlignment(VAlign_Center);
  FString N=FString::Printf(TEXT("ParticleCount%d"),I);auto* Input=Tree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(),FName(*N));Row->AddChildToHorizontalBox(Input)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
  auto* Field=NewObject<UProcessNumericField>(this);Field->Attach(W,*N,TEXT("개"),100,0,100000);ParticleCountFields.Add(Field);
 }
 auto Button=[&](const TCHAR* N,const TCHAR* Caption){auto* B=Tree->ConstructWidget<UButton>(UButton::StaticClass(),N);ProcessDisplay::MouseOnly(B);B->AddChild(Text(Caption));auto Style=B->GetStyle();Style.Normal=FSlateRoundedBoxBrush(FLinearColor(.83f,.90f,.95f),6.f);Style.Hovered=FSlateRoundedBoxBrush(FLinearColor(.70f,.83f,.92f),6.f);Style.Pressed=FSlateRoundedBoxBrush(FLinearColor(.58f,.75f,.87f),6.f);Style.NormalPadding=Style.PressedPadding=FMargin(12,10);B->SetStyle(Style);Box->AddChildToVerticalBox(B)->SetPadding(FMargin(0,8));return B;};
 Button(TEXT("ParticleClearDraft"),TEXT("전체 0개 입력 · 설비만 시험"))->OnClicked.AddDynamic(this,&UProcessControlUI::ClearParticleDraft);
 Button(TEXT("ParticleApply"),TEXT("입자 수 적용하고 초기화"))->OnClicked.AddDynamic(this,&UProcessControlUI::ApplyParticleSettings);
 Box->AddChildToVerticalBox(Text(TEXT(""),TEXT("ParticleSettingsMessage")));
 Button(TEXT("ParticleSettingsClose"),TEXT("공정 화면으로 돌아가기"))->OnClicked.AddDynamic(this,&UProcessControlUI::ToggleParticleSettings);
 ParticleSettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
}
void UProcessControlUI::ToggleParticleSettings(){
 ParticleSettingsOpen=!ParticleSettingsOpen;
 if(ParticleSettingsOpen){RecipeOpen=false;OperatorDetailsOpen=false;auto* S=Service.Get();auto* A=S?S->GetSource():nullptr;auto Counts=A?A->GetInitialParticleCounts():TArray<int32>();
  for(int I=0;I<ParticleCountFields.Num();++I){ParticleCountFields[I]->Input->SetText(FText::FromString(FString::FromInt(Counts.IsValidIndex(I)?Counts[I]:0)));ParticleCountFields[I]->Touched=false;}
  ParticleSettingsMessage=TEXT("설정은 이 PC에 저장되어 같은 설비의 다음 실행에도 적용됩니다.");
 }Refresh();
}
void UProcessControlUI::ClearParticleDraft(){for(auto F:ParticleCountFields)F->Input->SetText(FText::FromString(TEXT("0")));ParticleSettingsMessage=TEXT("0개를 입력했습니다. 적용 버튼을 눌러 초기화하세요.");Refresh();}
void UProcessControlUI::ApplyParticleSettings(){
 TArray<int32> Counts;
 for(auto F:ParticleCountFields){double V=0;if(!ProcessDisplay::Parse(F->Input->GetText().ToString(),V)||V<0||V>100000||V!=FMath::FloorToDouble(V)){ParticleSettingsMessage=TEXT("호퍼별로 0~100000 사이의 정수를 입력하세요.");Refresh();return;}Counts.Add(int32(V));}
 if(auto* S=Service.Get())S->ApplyParticleCounts(Counts,ParticleSettingsMessage);else ParticleSettingsMessage=TEXT("연결된 설비가 없습니다.");Refresh();
}
void UProcessControlUI::RefreshParticleControls(){
 if(!ParticleSettingsPanel)return;
 if(RecipeOpen||OperatorDetailsOpen)ParticleSettingsOpen=false;
 ParticleSettingsPanel->SetVisibility(ParticleSettingsOpen?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
 auto* S=Service.Get();auto* A=S?S->GetSource():nullptr;auto Counts=A?A->GetInitialParticleCounts():TArray<int32>();
 for(int I=0;I<ParticleCountFields.Num();++I){auto* F=ParticleCountFields[I].Get();double V=0;bool Integer=ProcessDisplay::Parse(F->Input->GetText().ToString(),V)&&V==FMath::FloorToDouble(V);F->Refresh(Counts.IsValidIndex(I)?Counts[I]:0,A!=nullptr,Integer?FString():TEXT("입자 수는 정수로 입력하세요."));}
 if(auto* W=Widget.Get()){
  if(auto* T=Cast<UTextBlock>(W->GetWidgetFromName(TEXT("ParticleSettingsMessage"))))T->SetText(FText::FromString(ParticleSettingsMessage));
  if(auto* B=Cast<UButton>(W->GetWidgetFromName(TEXT("ParticleApply"))))B->SetIsEnabled(A&&!S->WebOwnsControl());
 }
}
