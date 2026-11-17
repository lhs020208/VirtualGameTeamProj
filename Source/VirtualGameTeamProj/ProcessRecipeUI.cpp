#include "ProcessControlUI.h"
#include "ProcessControlService.h"
#include "ProcessDisplay.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include <cstdlib>
#include <limits>

namespace {
const TCHAR* Modes[]={TEXT("A 단독 전량"),TEXT("A 단독 목표량"),TEXT("A→B 순차 계량")};
FString TextOf(UUserWidget* W,const TCHAR* N){auto* I=Cast<UEditableTextBox>(W->GetWidgetFromName(N));return I?I->GetText().ToString():FString();}
double NumberOf(UUserWidget* W,const TCHAR* N){const FString T=TextOf(W,N).TrimStartAndEnd();FTCHARToUTF8 B(*T);char* End=nullptr;double V=std::strtod(B.Get(),&End);return T.IsEmpty()||End==B.Get()||!End||*End?std::numeric_limits<double>::quiet_NaN():V;}
bool Checked(UUserWidget* W,const TCHAR* N){auto* C=Cast<UCheckBox>(W->GetWidgetFromName(N));return C&&C->IsChecked();}
void Label(UUserWidget* W,const TCHAR* N,const FString& V){if(auto* T=Cast<UTextBlock>(W->GetWidgetFromName(N)))T->SetText(FText::FromString(V));}
}
void UProcessControlUI::BuildRecipeControls(UCanvasPanel* Root)
{
 auto* W=Widget.Get();auto* S=Service.Get();if(!W||!S||!Root)return;
 auto Text=[&](const TCHAR* N,const FString& V){auto* T=W->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),N);auto F=T->GetFont();F.Size=16;T->SetFont(F);T->SetColorAndOpacity(FLinearColor(.025f,.055f,.09f));T->SetAutoWrapText(true);T->SetText(FText::FromString(V));return T;};
 auto Button=[&](const TCHAR* N,const TCHAR* V){auto* B=W->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),N);B->AddChild(Text(TEXT(""),V));auto Style=B->GetStyle();Style.Normal=FSlateRoundedBoxBrush(FLinearColor(.76f,.88f,.94f),6);Style.NormalPadding=Style.PressedPadding=FMargin(10,7);B->SetStyle(Style);return B;};
 auto* Toggle=Button(TEXT("RecipeToggle"),TEXT("레시피"));GlobalControls->AddChildToHorizontalBox(Toggle)->SetPadding(FMargin(4,0));Toggle->OnClicked.AddDynamic(this,&UProcessControlUI::ToggleRecipes);
 RecipePanel=W->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("RecipePanel"));RecipePanel->SetBrushColor(FLinearColor(.94f,.97f,.98f,1));RecipePanel->SetPadding(FMargin(16));
 auto* Canvas=Root->AddChildToCanvas(RecipePanel);Canvas->SetAnchors(FAnchors(0,0,1,1));Canvas->SetOffsets(FMargin(20,72,20,20));Canvas->SetZOrder(30);
 auto* All=W->WidgetTree->ConstructWidget<UVerticalBox>();RecipePanel->SetContent(All);
 auto* Header=W->WidgetTree->ConstructWidget<UHorizontalBox>();All->AddChildToVerticalBox(Header);
 Header->AddChildToHorizontalBox(Text(TEXT("RecipeTitle"),TEXT("배치 레시피 · 편집 / 적용 / 파일 저장")))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
 auto* Close=Button(TEXT("RecipeClose"),TEXT("공정 화면으로"));Header->AddChildToHorizontalBox(Close);Close->OnClicked.AddDynamic(this,&UProcessControlUI::CloseRecipes);
 All->AddChildToVerticalBox(Text(TEXT("RecipeNotice"),TEXT("불러오기는 편집값만 바꿉니다. 전체 검증 후 적용하고, 시작 버튼으로 실행합니다. 원료 이름은 표시용이며 호퍼 매핑·물성은 바뀌지 않습니다.")))->SetPadding(FMargin(0,6));
 auto* Scroll=W->WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),TEXT("RecipeScroll"));All->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
 auto* P=W->WidgetTree->ConstructWidget<UVerticalBox>();Scroll->AddChild(P);
 auto* FileRow=W->WidgetTree->ConstructWidget<UHorizontalBox>();P->AddChildToVerticalBox(FileRow)->SetPadding(FMargin(0,5));
 auto* Files=W->WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(),TEXT("RecipeFileList"));Files->SetMaxListHeight(220);FileRow->AddChildToHorizontalBox(Files)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Files->OnSelectionChanged.AddDynamic(this,&UProcessControlUI::RecipeSelectionChanged);
 auto* Load=Button(TEXT("RecipeLoad"),TEXT("선택 레시피 불러오기"));FileRow->AddChildToHorizontalBox(Load)->SetPadding(FMargin(8,0));Load->OnClicked.AddDynamic(this,&UProcessControlUI::LoadSelectedRecipe);
 auto* Copy=Button(TEXT("RecipeCopyApplied"),TEXT("현재 적용값을 편집으로"));FileRow->AddChildToHorizontalBox(Copy);Copy->OnClicked.AddDynamic(this,&UProcessControlUI::CopyAppliedRecipe);
 auto Input=[&](const TCHAR* N,const TCHAR* Caption,UHorizontalBox* Row=nullptr,float Weight=1){
  auto* Col=W->WidgetTree->ConstructWidget<UVerticalBox>();if(Row){auto* Slot=Row->AddChildToHorizontalBox(Col);FSlateChildSize Size(ESlateSizeRule::Fill);Size.Value=Weight;Slot->SetSize(Size);Slot->SetPadding(FMargin(3,0));}else P->AddChildToVerticalBox(Col)->SetPadding(FMargin(0,4));
  Col->AddChildToVerticalBox(Text(TEXT(""),Caption));auto* I=W->WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(),N);auto Style=I->GetWidgetStyle();auto F=Style.TextStyle.Font;F.Size=18;Style.SetFont(F);Style.SetForegroundColor(FLinearColor(.025f,.055f,.09f));I->SetWidgetStyle(Style);I->OnTextChanged.AddDynamic(this,&UProcessControlUI::RecipeTextChanged);Col->AddChildToVerticalBox(I);
 };
 Input(TEXT("RecipeName"),TEXT("편집 레시피 이름"));
 auto* Mode=W->WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(),TEXT("RecipeMode"));for(const TCHAR* V:Modes)Mode->AddOption(V);Mode->OnSelectionChanged.AddDynamic(this,&UProcessControlUI::RecipeModeChanged);P->AddChildToVerticalBox(Mode)->SetPadding(FMargin(0,4));
 auto Material=[&](const TCHAR* RowName,const TCHAR* Caption,const TCHAR* Name,const TCHAR* Target,const TCHAR* Tol){
  auto* Row=W->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),RowName);P->AddChildToVerticalBox(Row)->SetPadding(FMargin(0,5));
  Input(Name,Caption,Row,2);Input(Target,TEXT("목표량 (g)"),Row);Input(Tol,TEXT("계량 오차 ± (g)"),Row);
 };
 Material(TEXT("RecipeARow"),TEXT("공급 A · 원료 표시명 (supply-a)"),TEXT("RecipeMaterialA"),TEXT("RecipeTargetA"),TEXT("RecipeToleranceA"));
 Material(TEXT("RecipeBRow"),TEXT("공급 B · 원료 표시명 (supply-b)"),TEXT("RecipeMaterialB"),TEXT("RecipeTargetB"),TEXT("RecipeToleranceB"));
 auto Check=[&](const TCHAR* N,const TCHAR* Caption,bool Draft){auto* C=W->WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(),N);C->AddChild(Text(TEXT(""),Caption));P->AddChildToVerticalBox(C)->SetPadding(FMargin(0,5));if(Draft)C->OnCheckStateChanged.AddDynamic(this,&UProcessControlUI::RecipeCheckChanged);return C;};
 Check(TEXT("RecipeAdditiveEnabled"),TEXT("C 첨가제 사용 · 현재 지원 호퍼 C 고정"),true);
 Material(TEXT("RecipeCRow"),TEXT("공급 C · 첨가제 표시명 (supply-c)"),TEXT("RecipeMaterialC"),TEXT("RecipeTargetC"),TEXT("RecipeToleranceC"));
 Check(TEXT("RecipeScheduled"),TEXT("예약 ON · 끄면 초기 투입(혼합 전) / ON·0초는 미리 준비 후 혼합 시작 직후 투입"),true);
 auto* MixRow=W->WidgetTree->ConstructWidget<UHorizontalBox>();P->AddChildToVerticalBox(MixRow)->SetPadding(FMargin(0,5));
 Input(TEXT("RecipeDelay"),TEXT("혼합 실제 시작 후 투입 N (해석초)"),MixRow);Input(TEXT("RecipeRPM"),TEXT("혼합 목표 RPM (0 초과 30 이하)"),MixRow);Input(TEXT("RecipeMixSeconds"),TEXT("혼합 시간 (해석초)"),MixRow);
 P->AddChildToVerticalBox(Text(TEXT("RecipePressureTitle"),TEXT("05  단계별 호퍼 목표 압력")))->SetPadding(FMargin(0,18,0,6));
 P->AddChildToVerticalBox(Text(TEXT("RecipePressureFixed"),TEXT("공급→칭량: 공급 A~F 공통 +0.5 / 칭량 -0.5 / 수취 0 kPa(g)\n칭량→수취: 공급 0 / 칭량 +0.5 kPa(g) · 아래 수취 목표만 변경\n밸브 앞뒤 추정값은 저장하지 않습니다. 실제 압력 명령은 배치의 해당 단계에서 적용됩니다.")));
 Input(TEXT("RecipeReceiverPressure"),TEXT("칭량→수취 단계 · 수취호퍼 목표 압력 (-1~0 kPa(g))"));
 P->AddChildToVerticalBox(Text(TEXT("RecipeOrder"),TEXT("지원 순서 확인")))->SetPadding(FMargin(0,5));
 P->AddChildToVerticalBox(Text(TEXT("RecipeDraftState"),TEXT("편집값 · 아직 적용하지 않음")))->SetPadding(FMargin(0,5));
 auto* Actions=W->WidgetTree->ConstructWidget<UHorizontalBox>();P->AddChildToVerticalBox(Actions)->SetPadding(FMargin(0,6));
 auto* Apply=Button(TEXT("RecipeApply"),TEXT("레시피 전체 검증·적용"));Actions->AddChildToHorizontalBox(Apply);Apply->OnClicked.AddDynamic(this,&UProcessControlUI::ApplyRecipeDraft);
 auto* Save=Button(TEXT("RecipeSaveNew"),TEXT("이 이름으로 새로 저장"));Actions->AddChildToHorizontalBox(Save)->SetPadding(FMargin(8,0));Save->OnClicked.AddDynamic(this,&UProcessControlUI::SaveNewRecipe);
 Check(TEXT("RecipeOverwriteConfirm"),TEXT("목록에서 선택한 같은 이름의 레시피를 덮어쓰겠습니다"),false);
 auto* Replace=Button(TEXT("RecipeOverwrite"),TEXT("선택 레시피 덮어쓰기"));P->AddChildToVerticalBox(Replace)->SetPadding(FMargin(0,4));Replace->OnClicked.AddDynamic(this,&UProcessControlUI::OverwriteSelectedRecipe);
 P->AddChildToVerticalBox(Text(TEXT("RecipeAppliedSummary"),TEXT("현재 적용값")))->SetPadding(FMargin(0,12));
 All->AddChildToVerticalBox(Text(TEXT("RecipeMessage"),TEXT("Saved/BatchRecipes · 버전2 JSON · 버전1 호환 · 저장/불러오기/적용은 장비를 조작하지 않습니다.")))->SetPadding(FMargin(0,6));
 RefreshRecipeList();FillRecipeDraft(S->GetAppliedRecipe());RecipePanel->SetVisibility(ESlateVisibility::Collapsed);
}
bool UProcessControlUI::RecipeEditable()
{
 auto* S=Service.Get();if(!S||Is3D()||S->AutoRun.State.Active||S->State.AutomationActive||S->State.IsStopping()){
  RecipeMessage=TEXT("실행/정지 처리 중 또는 3D 관찰 중에는 레시피를 변경하거나 불러올 수 없습니다. 현재 실행 설정을 유지합니다.");return false;
 }return true;
}
void UProcessControlUI::ToggleRecipes(){if(Is3D())return;RecipeOpen=!RecipeOpen;if(RecipeOpen)RefreshRecipeList();Refresh();}
void UProcessControlUI::CloseRecipes(){RecipeOpen=false;Refresh();}
void UProcessControlUI::RecipeModeChanged(FString,ESelectInfo::Type){if(!RecipeFilling){RecipeDirty=true;RefreshRecipeControls();}}
void UProcessControlUI::RecipeSelectionChanged(FString,ESelectInfo::Type){if(RecipeFilling)return;if(auto* W=Widget.Get())if(auto* C=Cast<UCheckBox>(W->GetWidgetFromName(TEXT("RecipeOverwriteConfirm"))))C->SetIsChecked(false);}
void UProcessControlUI::RecipeTextChanged(const FText&){if(!RecipeFilling){RecipeDirty=true;RecipeSelectionChanged(FString(),ESelectInfo::Direct);}}
void UProcessControlUI::RecipeCheckChanged(bool){if(!RecipeFilling){RecipeDirty=true;RefreshRecipeControls();}}
void UProcessControlUI::RefreshRecipeList(const FString& SelectedFile)
{
 auto* W=Widget.Get();auto* C=W?Cast<UComboBoxString>(W->GetWidgetFromName(TEXT("RecipeFileList"))):nullptr;if(!C)return;
 FString Keep=SelectedFile;const int32 Old=C->GetSelectedIndex();if(Keep.IsEmpty()&&RecipeFiles.IsValidIndex(Old))Keep=RecipeFiles[Old].File;
 TGuardValue<bool> Filling(RecipeFilling,true);RecipeFiles=FProcessRecipeStore().List();C->ClearOptions();
 for(const auto& F:RecipeFiles)C->AddOption(F.Name+(F.Error.IsEmpty()?FString():TEXT(" · 파일 오류")));
 for(int I=0;I<RecipeFiles.Num();++I)if(RecipeFiles[I].File==Keep){C->SetSelectedIndex(I);break;}
 if(C->GetSelectedIndex()<0&&RecipeFiles.Num())C->SetSelectedIndex(0);
}
void UProcessControlUI::FillRecipeDraft(const FProcessRecipe& R)
{
 auto* W=Widget.Get();if(!W)return;TGuardValue<bool> Filling(RecipeFilling,true);
 auto Put=[&](const TCHAR* N,const FString& V){if(auto* I=Cast<UEditableTextBox>(W->GetWidgetFromName(N)))I->SetText(FText::FromString(V));};
 auto Num=[&](const TCHAR* N,double V,bool BlankZero=false){Put(N,BlankZero&&V==0?FString():ProcessDisplay::Exact(V));};
 Put(TEXT("RecipeName"),R.Name);Put(TEXT("RecipeMaterialA"),R.MaterialA);Put(TEXT("RecipeMaterialB"),R.MaterialB);Put(TEXT("RecipeMaterialC"),R.MaterialC);
 Num(TEXT("RecipeReceiverPressure"),R.TransferPressurePa.Z/1000.);
 Num(TEXT("RecipeTargetA"),R.TargetAKg*1000,true);Num(TEXT("RecipeToleranceA"),R.ToleranceAKg*1000);Num(TEXT("RecipeTargetB"),R.TargetBKg*1000,true);Num(TEXT("RecipeToleranceB"),R.ToleranceBKg*1000);
 Num(TEXT("RecipeTargetC"),R.TargetCKg*1000,true);Num(TEXT("RecipeToleranceC"),R.ToleranceCKg*1000);Num(TEXT("RecipeRPM"),R.TargetRPM,true);Num(TEXT("RecipeMixSeconds"),R.MixingSeconds,true);Num(TEXT("RecipeDelay"),R.DelaySeconds);
 if(auto* C=Cast<UComboBoxString>(W->GetWidgetFromName(TEXT("RecipeMode"))))C->SetSelectedIndex(int32(R.Mode));
 for(const auto& Pair:TArray<TPair<FString,bool>>{{TEXT("RecipeAdditiveEnabled"),R.AdditiveEnabled},{TEXT("RecipeScheduled"),R.Scheduled},{TEXT("RecipeOverwriteConfirm"),false}})if(auto* C=Cast<UCheckBox>(W->GetWidgetFromName(FName(*Pair.Key))))C->SetIsChecked(Pair.Value);
 OriginalRecipeDraft=R;OriginalRecipeValid=true;OriginalRecipeText.Empty();
 for(const TCHAR* N:{TEXT("RecipeTargetA"),TEXT("RecipeToleranceA"),TEXT("RecipeTargetB"),TEXT("RecipeToleranceB"),TEXT("RecipeTargetC"),TEXT("RecipeToleranceC"),TEXT("RecipeRPM"),TEXT("RecipeMixSeconds"),TEXT("RecipeDelay"),TEXT("RecipeReceiverPressure")})OriginalRecipeText.Add(N,TextOf(W,N));
 RecipeDirty=true;
}
bool UProcessControlUI::ReadRecipeDraft(FProcessRecipe& Out,FString& Error) const
{
 auto* W=Widget.Get();if(!W){Error=TEXT("레시피 화면 연결 없음");return false;}auto* M=Cast<UComboBoxString>(W->GetWidgetFromName(TEXT("RecipeMode")));
 if(!M||M->GetSelectedIndex()<0||M->GetSelectedIndex()>2){Error=TEXT("지원 배치 모드를 선택하세요.");return false;}
 auto Native=[&](const TCHAR* N,double Original,double Scale){const FString* Text=OriginalRecipeText.Find(N);return OriginalRecipeValid&&Text&&*Text==TextOf(W,N)?Original:NumberOf(W,N)/Scale;};
 FProcessRecipe R;R.Name=TextOf(W,TEXT("RecipeName"));R.Mode=EProcessBatchMode(M->GetSelectedIndex());R.MaterialA=TextOf(W,TEXT("RecipeMaterialA"));
 if(R.Mode!=EProcessBatchMode::Full){R.TargetAKg=Native(TEXT("RecipeTargetA"),OriginalRecipeDraft.TargetAKg,1000.);R.ToleranceAKg=Native(TEXT("RecipeToleranceA"),OriginalRecipeDraft.ToleranceAKg,1000.);}
 if(R.Mode==EProcessBatchMode::SequentialAB){R.MaterialB=TextOf(W,TEXT("RecipeMaterialB"));R.TargetBKg=Native(TEXT("RecipeTargetB"),OriginalRecipeDraft.TargetBKg,1000.);R.ToleranceBKg=Native(TEXT("RecipeToleranceB"),OriginalRecipeDraft.ToleranceBKg,1000.);R.AdditiveEnabled=Checked(W,TEXT("RecipeAdditiveEnabled"));}
 if(R.AdditiveEnabled){R.MaterialC=TextOf(W,TEXT("RecipeMaterialC"));R.TargetCKg=Native(TEXT("RecipeTargetC"),OriginalRecipeDraft.TargetCKg,1000.);R.ToleranceCKg=Native(TEXT("RecipeToleranceC"),OriginalRecipeDraft.ToleranceCKg,1000.);R.Scheduled=Checked(W,TEXT("RecipeScheduled"));R.DelaySeconds=R.Scheduled?Native(TEXT("RecipeDelay"),OriginalRecipeDraft.DelaySeconds,1.):0;}
 R.TransferPressurePa.Z=Native(TEXT("RecipeReceiverPressure"),OriginalRecipeDraft.TransferPressurePa.Z,.001);
 R.TargetRPM=Native(TEXT("RecipeRPM"),OriginalRecipeDraft.TargetRPM,1.);R.MixingSeconds=Native(TEXT("RecipeMixSeconds"),OriginalRecipeDraft.MixingSeconds,1.);Error=ProcessRecipe::Validate(R);if(!Error.IsEmpty())return false;Out=R;return true;
}
void UProcessControlUI::LoadSelectedRecipe()
{
 if(!RecipeEditable()){Refresh();return;}auto* W=Widget.Get();auto* C=Cast<UComboBoxString>(W->GetWidgetFromName(TEXT("RecipeFileList")));const int I=C?C->GetSelectedIndex():-1;
 FProcessRecipe R;if(!RecipeFiles.IsValidIndex(I))RecipeMessage=TEXT("불러올 레시피를 선택하세요.");
 else if(FProcessRecipeStore().Load(RecipeFiles[I].File,R,RecipeMessage)){FillRecipeDraft(R);RecipeMessage=TEXT("불러오기 완료 · 편집값만 변경했습니다. 전체 적용 후 시작하세요.");}
 Refresh();
}
void UProcessControlUI::CopyAppliedRecipe(){if(RecipeEditable())if(auto* S=Service.Get()){FillRecipeDraft(S->GetAppliedRecipe());RecipeMessage=TEXT("현재 적용값을 편집으로 복사했습니다. 장비 명령은 보내지 않았습니다.");}Refresh();}
void UProcessControlUI::ApplyRecipeDraft()
{
 if(RecipeEditable()){FProcessRecipe R;if(ReadRecipeDraft(R,RecipeMessage))if(auto* S=Service.Get())if(S->ApplyRecipe(R,RecipeMessage)){RecipeDirty=false;RecipeMessage=TEXT("레시피 전체 적용 완료 · 장비 명령 없음. 시작 시 재고·입자 질량을 다시 확인하고 이번 실행값으로 확정합니다.");}}
 Refresh();
}
void UProcessControlUI::SaveNewRecipe()
{
 if(RecipeEditable()){FProcessRecipe R;FString File;if(ReadRecipeDraft(R,RecipeMessage)&&FProcessRecipeStore().SaveNew(R,File,RecipeMessage)){RefreshRecipeList(File);RecipeMessage=TEXT("새 레시피 저장 완료 · 편집값을 저장했으며 적용값/장비 상태는 바꾸지 않았습니다.");}}
 Refresh();
}
void UProcessControlUI::OverwriteSelectedRecipe()
{
 if(!RecipeEditable()){Refresh();return;}auto* W=Widget.Get();auto* C=Cast<UComboBoxString>(W->GetWidgetFromName(TEXT("RecipeFileList")));const int I=C?C->GetSelectedIndex():-1;
 if(!RecipeFiles.IsValidIndex(I)||!Checked(W,TEXT("RecipeOverwriteConfirm")))RecipeMessage=TEXT("목록에서 레시피를 선택하고 덮어쓰기 확인란을 직접 선택하세요.");
 else {FProcessRecipe R;if(ReadRecipeDraft(R,RecipeMessage)&&FProcessRecipeStore().ReplaceSelected(RecipeFiles[I].File,R,RecipeMessage)){const FString File=RecipeFiles[I].File;RefreshRecipeList(File);RecipeMessage=TEXT("선택 레시피 덮어쓰기 완료 · 적용값/장비 상태는 유지했습니다.");}}
 RecipeSelectionChanged(FString(),ESelectInfo::Direct);Refresh();
}
void UProcessControlUI::RefreshRecipeControls()
{
 auto* W=Widget.Get();auto* S=Service.Get();if(!W||!S||!RecipePanel)return;
 if(auto* B=W->GetWidgetFromName(TEXT("RecipeToggle")))B->SetVisibility(Is3D()?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
 RecipePanel->SetVisibility(RecipeOpen&&!Is3D()?ESlateVisibility::Visible:ESlateVisibility::Collapsed);if(!RecipeOpen||Is3D())return;
 const bool Edit=!S->AutoRun.State.Active&&!S->State.AutomationActive&&!S->State.IsStopping();auto* M=Cast<UComboBoxString>(W->GetWidgetFromName(TEXT("RecipeMode")));
 const int Mode=M?M->GetSelectedIndex():-1;const bool AB=Mode==2,C=AB&&Checked(W,TEXT("RecipeAdditiveEnabled")),Scheduled=C&&Checked(W,TEXT("RecipeScheduled"));
 for(const TCHAR* N:{TEXT("RecipeName"),TEXT("RecipeMode"),TEXT("RecipeMaterialA"),TEXT("RecipeMaterialB"),TEXT("RecipeMaterialC"),TEXT("RecipeTargetA"),TEXT("RecipeToleranceA"),TEXT("RecipeTargetB"),TEXT("RecipeToleranceB"),TEXT("RecipeTargetC"),TEXT("RecipeToleranceC"),TEXT("RecipeAdditiveEnabled"),TEXT("RecipeScheduled"),TEXT("RecipeDelay"),TEXT("RecipeRPM"),TEXT("RecipeMixSeconds"),TEXT("RecipeReceiverPressure"),TEXT("RecipeFileList"),TEXT("RecipeLoad"),TEXT("RecipeCopyApplied"),TEXT("RecipeApply"),TEXT("RecipeSaveNew"),TEXT("RecipeOverwrite"),TEXT("RecipeOverwriteConfirm")})if(auto* V=W->GetWidgetFromName(N))V->SetIsEnabled(Edit);
 for(const TCHAR* N:{TEXT("RecipeTargetA"),TEXT("RecipeToleranceA")})if(auto* V=W->GetWidgetFromName(N))V->SetIsEnabled(Edit&&Mode!=0);
 if(auto* V=W->GetWidgetFromName(TEXT("RecipeBRow")))V->SetVisibility(AB?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
 if(auto* V=W->GetWidgetFromName(TEXT("RecipeAdditiveEnabled")))V->SetVisibility(AB?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
 if(auto* V=W->GetWidgetFromName(TEXT("RecipeCRow")))V->SetVisibility(C?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
 if(auto* V=W->GetWidgetFromName(TEXT("RecipeScheduled")))V->SetVisibility(C?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
 if(auto* V=W->GetWidgetFromName(TEXT("RecipeDelay")))V->SetIsEnabled(Edit&&Scheduled);
 Label(W,TEXT("RecipeOrder"),AB?TEXT("지원 순서: A 계량·수취 보관 → B 계량·수취 보관 → A·B 합산 혼합기 투입. C 사용 시 이후 개별 계량·투입. 임의 재정렬/미지원 호퍼는 실행할 수 없습니다."):TEXT("지원 순서: A → 칭량호퍼 → 수취호퍼 → 혼합기. 전량 모드의 목표량/오차는 사용하지 않습니다."));
 const auto Applied=S->GetAppliedRecipe();const bool Modified=S->RecipeRevision>0&&!ProcessRecipe::Same(Applied,S->RecipeMetadata);
 Label(W,TEXT("RecipeDraftState"),!Edit?TEXT("실행 중 · 레시피 편집/불러오기/적용 잠금 · 전체 정지는 하단에서 가능"):(RecipeDirty||Modified)?TEXT("편집값 · 적용되지 않은 변경이 있습니다. 저장과 적용은 별도입니다."):TEXT("편집값 전체를 적용했습니다. 시작 버튼은 적용된 값으로 실행합니다."));
 Label(W,TEXT("RecipeAppliedSummary"),TEXT("현재 적용값 · 시작 버튼이 사용할 설정")+(Modified?FString(TEXT(" (개별 설정에서 변경됨)")):FString())+TEXT("\n")+ProcessRecipe::Summary(Applied));
 Label(W,TEXT("RecipeMessage"),RecipeMessage.IsEmpty()?TEXT("Saved/BatchRecipes · 버전2 JSON · 버전1 호환 · 예약 N은 목표 RPM 실제 적용 후 수취 출구를 열 시각입니다."):RecipeMessage);
}
