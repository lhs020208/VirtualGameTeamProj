#include "ProcessControlUI.h"
#include "ProcessControlService.h"
#include "ProcessNumericField.h"
#include "ProcessDisplay.h"
#include "ProcessPlantBinding.h"
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
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "HAL/PlatformTime.h"
#include <limits>

namespace {
const FLinearColor Ink(.035f,.075f,.12f),Muted(.23f,.32f,.40f),Blue(.02f,.30f,.48f),Red(.65f,.045f,.055f);
void Put(UUserWidget* W,const TCHAR* N,const FString& V){if(auto* T=Cast<UTextBlock>(W->GetWidgetFromName(N)))T->SetText(FText::FromString(V));}
void Font(UTextBlock* T,int32 Size,FLinearColor Color=Ink){if(!T)return;auto F=T->GetFont();F.Size=Size;T->SetFont(F);T->SetColorAndOpacity(Color);T->SetAutoWrapText(true);}
void Style(UButton* B,int32 Size=22,bool Primary=false,bool Stop=false){
 if(!B)return;ProcessDisplay::MouseOnly(B);Font(Cast<UTextBlock>(B->GetContent()),Size,Primary||Stop?FLinearColor::White:Ink);
 if(auto* T=Cast<UTextBlock>(B->GetContent()))T->SetAutoWrapText(false);
 const FLinearColor Fill=Stop?Red:Primary?Blue:FLinearColor(.86f,.92f,.96f);
 auto S=B->GetStyle();S.Normal=FSlateRoundedBoxBrush(Fill,8.f);S.Hovered=FSlateRoundedBoxBrush(Fill*.9f,8.f);S.Pressed=FSlateRoundedBoxBrush(Fill*.8f,8.f);S.Disabled=FSlateRoundedBoxBrush(FLinearColor(.60f,.67f,.71f),8.f);S.NormalPadding=S.PressedPadding=FMargin(16,12);B->SetStyle(S);B->SetBackgroundColor(FLinearColor::White);
}
FString Stage(const FProcessAutoSnapshot& A){
 const FString Source=FString::Chr(TCHAR('A'+A.ActiveSupplier));
 switch(A.Stage){
 case EProcessAutoStage::Complete:return TEXT("배치 완료");
 case EProcessAutoStage::Feeding:case EProcessAutoStage::OpenA:return Source+TEXT(" 계량 중");
 case EProcessAutoStage::CloseA:return Source+TEXT(" 공급 밸브 닫힘 확인 중");
 case EProcessAutoStage::Clearing:case EProcessAutoStage::PurgeStart:case EProcessAutoStage::PurgePressure:return Source+TEXT(" 공급 배관 비우는 중");
 case EProcessAutoStage::WeighingStable:case EProcessAutoStage::FinalizeDose:return Source+TEXT(" 계량 안정 확인 중");
 case EProcessAutoStage::OpenWeigher:return Source+TEXT(" 공급 완료 재확인 중");
 case EProcessAutoStage::Transferring:return Source+TEXT(" 수취호퍼로 이송 중");
 case EProcessAutoStage::FeedingMixer:return A.IsAdditive()?Source+TEXT(" 첨가제 투입 중"):(A.BatchMode==EProcessBatchMode::SequentialAB?TEXT("합산 원료 혼합기로 투입 중"):TEXT("A 혼합기로 투입 중"));
 case EProcessAutoStage::WaitAdditive:return Source+TEXT(" 예약 투입 대기");
 case EProcessAutoStage::Mixing:return TEXT("혼합 중");
 case EProcessAutoStage::StopMixer:return TEXT("혼합기 정지 확인 중");
 case EProcessAutoStage::Cancelled:return TEXT("배치 중단 · 상태 확인 필요");
 case EProcessAutoStage::Failed:return TEXT("배치 실패 · 조치 필요");
 case EProcessAutoStage::Stopping:return TEXT("전체 정지 확인 중");
 default:return FProcessAutoRun::StageText(A.Stage);
 }
}
}

void UProcessControlUI::BuildOperatorLayout(UCanvasPanel* Root)
{
 auto* W=Widget.Get();if(!W||OperatorLayoutBuilt)return;auto* Tree=W->WidgetTree.Get();
 TArray<UWidget*> All;Tree->GetAllWidgets(All);TMap<FString,UWidget*> Widgets;for(auto* X:All)Widgets.Add(X->GetName(),X);
 auto Get=[&](const TCHAR* N){return Widgets.FindRef(N);};
 auto Text=[&](const TCHAR* N,const FString& V,int32 Size=22){auto* T=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),N);Font(T,Size);T->SetText(FText::FromString(V));return T;};
 auto Button=[&](const TCHAR* N,const TCHAR* Caption){auto* B=Tree->ConstructWidget<UButton>(UButton::StaticClass(),N);B->AddChild(Text(TEXT(""),Caption));Style(B);return B;};
 auto Move=[&](UWidget* X,UVerticalBox* P){if(!X)return;X->RemoveFromParent();P->AddChildToVerticalBox(X)->SetPadding(FMargin(0,5));};
 auto Canvas=[&](UWidget* X,FAnchors A,FMargin O,int Z){X->RemoveFromParent();auto* S=Root->AddChildToCanvas(X);S->SetAnchors(A);S->SetOffsets(O);S->SetZOrder(Z);};
 // Navigation, current state and the stop action occupy fixed bands outside all scrolling content.
 auto* Header=Cast<UBorder>(Get(TEXT("Header")));auto* Title=Get(TEXT("Title"));if(Title)Title->RemoveFromParent();
 auto* Nav=Tree->ConstructWidget<UHorizontalBox>();Header->SetContent(Nav);Header->SetPadding(FMargin(22,10));
 Canvas(Header,FAnchors(0,0,1,0),FMargin(0,0,0,80),50);
 if(Title){Nav->AddChildToHorizontalBox(Title)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Put(W,TEXT("Title"),TEXT("분체 공정 · 운전"));Font(Cast<UTextBlock>(Title),28,FLinearColor::White);}
 for(const TCHAR* N:{TEXT("View2D"),TEXT("View3D"),TEXT("RecipeToggle")})if(auto* X=Get(N)){X->RemoveFromParent();auto* S=Nav->AddChildToHorizontalBox(X);S->SetPadding(FMargin(5,0));Style(Cast<UButton>(X));}
 auto* Details=Button(TEXT("OperatorDetailsToggle"),TEXT("상세 보기"));Nav->AddChildToHorizontalBox(Details)->SetPadding(FMargin(5,0));Details->OnClicked.AddDynamic(this,&UProcessControlUI::ToggleOperatorDetails);
 auto* StatePanel=Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("OperatorStatePanel"));StatePanel->SetPadding(FMargin(20,10));
 Canvas(StatePanel,FAnchors(0,0,1,0),FMargin(20,90,20,128),5);
 auto* StateBox=Tree->ConstructWidget<UVerticalBox>();StatePanel->SetContent(StateBox);
 StateBox->AddChildToVerticalBox(Text(TEXT("OperatorStage"),TEXT("연결 확인 중"),28));
 StateBox->AddChildToVerticalBox(Text(TEXT("OperatorReason"),TEXT("작업자 상태를 기다립니다."),20));
 if(auto* Help=Get(TEXT("Help")))Help->SetVisibility(ESlateVisibility::Collapsed);

 auto* Body=Cast<UBorder>(Get(TEXT("BodyBackground")));Canvas(Body,FAnchors(0,0,1,1),FMargin(20,230,20,166),1);Body->SetPadding(FMargin(18));Body->SetBrushColor(FLinearColor(.95f,.97f,.985f));
 auto* Columns=Tree->ConstructWidget<UHorizontalBox>();
 auto* Design=Cast<USizeBox>(Get(TEXT("DesignSize")));Design->RemoveFromParent();Design->SetWidthOverride(1100);Design->SetHeightOverride(730);
 auto* Panel=Cast<UVerticalBox>(Get(TEXT("PanelContents")));Panel->RemoveFromParent();
 Body->SetContent(Columns);
 auto* Scale=Tree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(),TEXT("OperatorDiagramScale"));Scale->SetStretch(EStretch::ScaleToFit);Scale->SetContent(Design);
 FSlateChildSize LeftSize(ESlateSizeRule::Fill);LeftSize.Value=1.3f;Columns->AddChildToHorizontalBox(Scale)->SetSize(LeftSize);
 auto* Side=Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),TEXT("OperatorSideScroll"));Side->SetScrollbarThickness(FVector2D(12,12));
 auto* SideSlot=Columns->AddChildToHorizontalBox(Side);SideSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));SideSlot->SetPadding(FMargin(20,0,0,0));
 auto* Right=Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("OperatorSideContent"));Side->AddChild(Right);
 Right->AddChildToVerticalBox(Text(TEXT("SelectedOverview"),TEXT(""),24))->SetPadding(FMargin(0,0,0,5));
 Move(PressureControls,Right);
 for(const TCHAR* N:{TEXT("RPMLabel"),TEXT("RPMInput"),TEXT("SetRPM")})Move(Get(N),Right);
 Right->AddChildToVerticalBox(Panel);
 Right->AddChildToVerticalBox(Text(TEXT("OperatorSummary"),TEXT("질량 확인 중"),25))->SetPadding(FMargin(0,0,0,16));
 auto* CalcRow=Tree->ConstructWidget<UHorizontalBox>();Right->AddChildToVerticalBox(CalcRow)->SetPadding(FMargin(0,5));
 for(const TCHAR* N:{TEXT("ManualResume"),TEXT("ManualPause")})if(auto* X=Get(N)){X->RemoveFromParent();auto* S=CalcRow->AddChildToHorizontalBox(X);S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));S->SetPadding(FMargin(3,0));}
 auto* RecoveryRow=Tree->ConstructWidget<UHorizontalBox>();Right->AddChildToVerticalBox(RecoveryRow)->SetPadding(FMargin(0,5));
 for(const TCHAR* N:{TEXT("CheckBatch"),TEXT("AbandonBatch")})if(auto* X=Get(N)){X->RemoveFromParent();auto* S=RecoveryRow->AddChildToHorizontalBox(X);S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));S->SetPadding(FMargin(3,0));}
 Right->AddChildToVerticalBox(Text(TEXT("OperatorSelectHint"),TEXT("공정도에서 장비를 클릭하면 관련 수동 조작을 볼 수 있습니다."),20))->SetPadding(FMargin(0,16));
 for(const TCHAR* N:{TEXT("PanelBlocker"),TEXT("HoverCard"),TEXT("SelectHint"),TEXT("DiagramNote")})if(auto* X=Get(N))X->SetVisibility(ESlateVisibility::Collapsed);
 for(int I=0;I<9;++I){Font(Cast<UTextBlock>(Get(*FString::Printf(TEXT("Name%d"),I))),25);Font(Cast<UTextBlock>(W->GetWidgetFromName(FName(*FString::Printf(TEXT("Equipment%dText"),I)))),24);}

 auto* Footer=Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("OperatorActionPanel"));Footer->SetPadding(FMargin(16,8));Footer->SetBrushColor(FLinearColor(.04f,.085f,.13f));
 Canvas(Footer,FAnchors(0,1,1,1),FMargin(20,-154,20,136),100);
 GlobalControls->ClearChildren();GlobalControls->RemoveFromParent();Footer->SetContent(GlobalControls);
 for(const TCHAR* N:{TEXT("AutoStartA"),TEXT("ResumeBatch")})if(auto* X=Get(N)){X->RemoveFromParent();auto* Box=Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),FName(*(FString(N)+TEXT("Container"))));Box->SetWidthOverride(270);Box->SetContent(X);GlobalControls->AddChildToHorizontalBox(Box)->SetPadding(FMargin(4,0));}
 auto* FooterHint=Text(TEXT("OperatorActionHint"),TEXT("설정 적용 후 시작 · 전체 정지는 언제든 가능"),18);Font(FooterHint,18,FLinearColor::White);
 auto* Guidance=Tree->ConstructWidget<UVerticalBox>();auto* HS=GlobalControls->AddChildToHorizontalBox(Guidance);HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));HS->SetPadding(FMargin(18,0));HS->SetVerticalAlignment(VAlign_Center);
 Guidance->AddChildToVerticalBox(FooterHint);auto* Next=Button(TEXT("OperatorOpenRecipe"),TEXT("레시피 설정 열기"));Guidance->AddChildToVerticalBox(Next)->SetPadding(FMargin(0,4,0,0));Next->OnClicked.AddDynamic(this,&UProcessControlUI::OpenRecipeSettings);
 if(auto* Check=Get(TEXT("CheckBatch"))){Check->RemoveFromParent();Guidance->AddChildToVerticalBox(Check)->SetPadding(FMargin(0,4,0,0));}
 if(auto* X=Get(TEXT("GlobalStop"))){X->RemoveFromParent();auto* Box=Tree->ConstructWidget<USizeBox>();Box->SetWidthOverride(230);Box->SetContent(X);GlobalControls->AddChildToHorizontalBox(Box)->SetPadding(FMargin(24,0,0,0));}

 // Settings remain the existing recipe editor and service. Reorder and group only its widgets.
 Canvas(RecipePanel,FAnchors(0,0,1,1),FMargin(20,230,20,166),40);
 auto* RecipeAll=Cast<UVerticalBox>(RecipePanel->GetContent());
 auto* Applied=Text(TEXT("OperatorAppliedRecipe"),TEXT("현재 배치에 사용할 적용값"),20);
 RecipeAll->InsertChildAt(1,Applied);
 // Applying and returning remain two explicit actions, always visible outside the settings scroll.
 auto* ApplyBar=Tree->ConstructWidget<UHorizontalBox>();RecipeAll->AddChildToVerticalBox(ApplyBar)->SetPadding(FMargin(0,6));
 if(auto* X=Get(TEXT("RecipeApply"))){X->RemoveFromParent();ApplyBar->AddChildToHorizontalBox(X);}
 auto* Back=Button(TEXT("OperatorRecipeReturn"),TEXT("공정 화면으로 돌아가기"));ApplyBar->AddChildToHorizontalBox(Back)->SetPadding(FMargin(12,0));Back->OnClicked.AddDynamic(this,&UProcessControlUI::CloseRecipes);
 auto* NameColumn=Get(TEXT("RecipeName"))->GetParent();auto* Settings=Cast<UVerticalBox>(NameColumn->GetParent());
 auto* DelayColumn=Get(TEXT("RecipeDelay"))->GetParent();auto* MixRow=DelayColumn->GetParent();DelayColumn->RemoveFromParent();
 auto* FileRow=Get(TEXT("RecipeFileList"))->GetParent();const auto Children=Settings->GetAllChildren();Settings->ClearChildren();
 auto Section=[&](const TCHAR* N,const TCHAR* Caption){auto* T=Text(N,Caption,26);Font(T,26,Blue);Settings->AddChildToVerticalBox(T)->SetPadding(FMargin(0,18,0,8));};
 for(auto* Child:Children){
  if(Child==FileRow)Section(TEXT("RecipeSection1"),TEXT("01  레시피"));
  if(Child==Get(TEXT("FeedSlot0")))Section(TEXT("RecipeSection2"),TEXT("02  공급호퍼 · 목표량과 투입 순서"));
  if(Child==Get(TEXT("RecipeAdditiveEnabled")))Section(TEXT("RecipeSection3"),TEXT("03  첨가제 · 투입 시점"));
  if(Child==MixRow){Settings->AddChildToVerticalBox(DelayColumn)->SetPadding(FMargin(4,8));Section(TEXT("RecipeSection4"),TEXT("04  혼합 설정"));}
  Settings->AddChildToVerticalBox(Child)->SetPadding(FMargin(2,5));
 }
 for(const TCHAR* N:{TEXT("RecipeARow"),TEXT("RecipeBRow"),TEXT("RecipeCRow")}){
  auto* Row=Cast<UHorizontalBox>(Get(N));const auto Items=Row->GetAllChildren();Row->ClearChildren();
  auto* Group=Tree->ConstructWidget<UVerticalBox>();Row->AddChildToHorizontalBox(Group)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
  Group->AddChildToVerticalBox(Items[0])->SetPadding(FMargin(0,0,0,8));auto* Values=Tree->ConstructWidget<UHorizontalBox>();Group->AddChildToVerticalBox(Values);
  for(int I=1;I<Items.Num();++I){auto* Slot=Values->AddChildToHorizontalBox(Items[I]);Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Slot->SetPadding(FMargin(0,0,20,0));}
 }
 for(const TCHAR* N:{TEXT("RecipeName"),TEXT("RecipeMaterialA"),TEXT("RecipeMaterialB"),TEXT("RecipeMaterialC")})if(auto* I=Cast<UEditableTextBox>(Get(N))){auto Style=I->GetWidgetStyle();auto F=Style.TextStyle.Font;F.Size=22;Style.SetFont(F);Style.Padding=FMargin(10,8);I->SetWidgetStyle(Style);}
 auto* LegacyToggle=Button(TEXT("IndividualSettingsToggle"),TEXT("개별 설정 펼치기 · 기존 조작"));Settings->AddChildToVerticalBox(LegacyToggle)->SetPadding(FMargin(0,16));LegacyToggle->OnClicked.AddDynamic(this,&UProcessControlUI::ToggleIndividualSettings);
 Move(DoseControls,Settings);Move(MixTimeControls,Settings);

 OperatorDetails=Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("OperatorDetailsPanel"));OperatorDetails->SetPadding(FMargin(24));OperatorDetails->SetBrushColor(FLinearColor(.97f,.98f,1));
 Canvas(OperatorDetails,FAnchors(.08f,0,.92f,1),FMargin(0,230,0,166),60);
 auto* DetailBox=Tree->ConstructWidget<UVerticalBox>();OperatorDetails->SetContent(DetailBox);
 auto* CloseDetails=Button(TEXT("OperatorDetailsClose"),TEXT("상세 닫기"));CloseDetails->OnClicked.AddDynamic(this,&UProcessControlUI::ToggleOperatorDetails);DetailBox->AddChildToVerticalBox(CloseDetails)->SetPadding(FMargin(0,0,0,12));
 auto* DetailScroll=Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),TEXT("OperatorDetailsScroll"));DetailBox->AddChildToVerticalBox(DetailScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
 auto* DetailContent=Tree->ConstructWidget<UVerticalBox>();DetailScroll->AddChild(DetailContent);
 DetailContent->AddChildToVerticalBox(Text(TEXT("OperatorExactValues"),TEXT("정밀 수치 · 작업자 확정 스냅샷"),21));
 for(const TCHAR* N:{TEXT("TransferTelemetry"),TEXT("PressureHelp"),TEXT("PurgeHelp"),TEXT("DataSource")})Move(Get(N),DetailContent);
 OperatorDetails->SetVisibility(ESlateVisibility::Collapsed);
 // Uniform text size for settings. Draft text is kept exact and is never rounded on refresh.
 Tree->GetAllWidgets(All);for(auto* X:All){if(auto* B=Cast<UButton>(X))Style(B);if(auto* T=Cast<UTextBlock>(X))if(T->GetName().StartsWith(TEXT("Recipe")))Font(T,T->GetName().StartsWith(TEXT("RecipeSection"))?26:21);}
 for(auto* X:All){bool InSettings=false;for(auto* P=X->GetParent();P;P=P->GetParent())if(P==RecipePanel){InSettings=true;break;}
  if(InSettings)if(auto* T=Cast<UTextBlock>(X))Font(T,T->GetName().StartsWith(TEXT("RecipeSection"))?26:21);
 }
 for(const TCHAR* N:{TEXT("RecipeMode"),TEXT("RecipeFileList")})if(auto* C=Cast<UComboBoxString>(Get(N)))C->SetContentPadding(FMargin(12,12));
 const double Unlimited=std::numeric_limits<double>::max();
 auto Numeric=[&](const TCHAR* N,const TCHAR* Unit,double Step,double Min=0,double Max=std::numeric_limits<double>::max(),bool Positive=false){
  auto* F=NewObject<UProcessNumericField>(this);if(F->Attach(W,N,Unit,Step,Min,Max,Positive))NumericFields.Add(F);
 };
 for(const TCHAR* N:{TEXT("RecipeTargetA"),TEXT("RecipeTargetB"),TEXT("RecipeTargetC"),TEXT("TargetMassInput"),TEXT("BTargetMassInput"),TEXT("AdditiveTargetInput")})Numeric(N,TEXT("g"),1,0,Unlimited,true);
 for(const TCHAR* N:{TEXT("RecipeToleranceA"),TEXT("RecipeToleranceB"),TEXT("RecipeToleranceC"),TEXT("WeighToleranceInput"),TEXT("BWeighToleranceInput"),TEXT("AdditiveToleranceInput")})Numeric(N,TEXT("g"),.1);
 for(const TCHAR* N:{TEXT("RecipeDelay"),TEXT("AdditiveDelayInput")})Numeric(N,TEXT("초"),.1);
 for(const TCHAR* N:{TEXT("RecipeMixSeconds"),TEXT("AutoMixTimeInput")})Numeric(N,TEXT("초"),1,0,Unlimited,true);
 for(int I=0;I<6;++I){
  const FString Slot=FString::Printf(TEXT("FeedSlot%d"),I);
  Numeric(*(Slot+TEXT("Target")),TEXT("g"),1,0,Unlimited,true);
  Numeric(*(Slot+TEXT("Tolerance")),TEXT("g"),.1);
  Numeric(*FString::Printf(TEXT("RecipeSourcePressure%d"),I),TEXT("kPa(g)"),.1,-1,1);
 }
 Numeric(TEXT("RecipeRPM"),TEXT("RPM"),1,0,30,true);Numeric(TEXT("RPMInput"),TEXT("RPM"),1,0,30);
 Numeric(TEXT("RecipeReceiverPressure"),TEXT("kPa(g)"),.1,-1,0);
 Numeric(TEXT("PressureInput"),TEXT("kPa(g)"),.1,-1,1);Numeric(TEXT("PurgePressureInput"),TEXT("kPa(g)"),.1,0,1);
 BuildResponsiveOperatorLayout(Root);
 OperatorLayoutBuilt=true;
}

void UProcessControlUI::ToggleOperatorDetails(){OperatorDetailsOpen=!OperatorDetailsOpen;Refresh();}
void UProcessControlUI::ToggleIndividualSettings(){IndividualSettingsOpen=!IndividualSettingsOpen;Refresh();}

void UProcessControlUI::RefreshOperatorLayout()
{
 auto* W=Widget.Get();auto* S=Service.Get();if(!W||!S)return;
 if(!OperatorLayoutBuilt){if(ProcessPlantBinding::IsLowPoly(S->State.Snapshot.SourceActorPath))BuildOperatorLayout(Cast<UCanvasPanel>(W->WidgetTree->RootWidget));else return;}
 const auto& M=S->State;const auto& Snap=M.Snapshot;const auto& A=S->AutoRun.State;const auto R=S->GetAppliedRecipe();
 const bool In3D=Is3D(),Fresh=M.Connected&&Snap.Validity==EPlantValueValidity::Valid&&FPlatformTime::Seconds()>=Snap.PublishedWallSeconds&&FPlatformTime::Seconds()-Snap.PublishedWallSeconds<=2.;
 const bool Same=A.Epoch==Snap.Epoch&&A.Connection==M.Connection,Complete=Same&&A.Stage==EProcessAutoStage::Complete&&!A.Active;
 auto Show=[&](const TCHAR* N,bool B){if(auto* X=W->GetWidgetFromName(N))X->SetVisibility(B?ESlateVisibility::Visible:ESlateVisibility::Collapsed);};
 Show(TEXT("Help"),false);Show(TEXT("BodyBackground"),!In3D&&!RecipeOpen);GlobalControls->SetVisibility(ESlateVisibility::Visible);
 Show(TEXT("PanelContents"),!Selected().IsEmpty());Show(TEXT("OperatorSelectHint"),Selected().IsEmpty());
 for(const TCHAR* N:{TEXT("PanelBlocker"),TEXT("HoverCard"),TEXT("SelectHint"),TEXT("DiagramNote"),TEXT("TransferStart"),TEXT("TransferStop"),TEXT("PanelDescription"),TEXT("PanelState")})Show(N,false);
 OperatorDetails->SetVisibility(OperatorDetailsOpen?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
 Show(TEXT("AutoDoseControls"),RecipeOpen&&IndividualSettingsOpen);Show(TEXT("AutoMixTimeControls"),RecipeOpen&&IndividualSettingsOpen);
 if(auto* B=Cast<UButton>(W->GetWidgetFromName(TEXT("IndividualSettingsToggle"))))if(auto* T=Cast<UTextBlock>(B->GetContent()))T->SetText(FText::FromString(IndividualSettingsOpen?TEXT("개별 설정 접기"):TEXT("개별 설정 펼치기 · 기존 조작")));
 if(auto* B=Cast<UButton>(W->GetWidgetFromName(TEXT("RecipeToggle"))))if(auto* T=Cast<UTextBlock>(B->GetContent()))T->SetText(FText::FromString(TEXT("설정 · 레시피")));
 for(const TCHAR* N:{TEXT("View2D"),TEXT("View3D"),TEXT("RecipeToggle"),TEXT("OperatorDetailsToggle"),TEXT("ValveOpen"),TEXT("ValveClose"),TEXT("ManualResume"),TEXT("ManualPause")})Style(Cast<UButton>(W->GetWidgetFromName(N)),21);
 Style(Cast<UButton>(W->GetWidgetFromName(TEXT("AutoStartA"))),25,true);Style(Cast<UButton>(W->GetWidgetFromName(TEXT("ResumeBatch"))),25,true);Style(Cast<UButton>(W->GetWidgetFromName(TEXT("GlobalStop"))),28,false,true);
 FString Title=A.RunId.IsValid()&&Same?Stage(A):(S->RecipeRevision>0?TEXT("시작 대기 · 레시피 적용됨"):TEXT("설정 대기 · 레시피를 적용하세요"));
 FString Why=A.RunId.IsValid()&&Same?(A.RecoveryPending&&!A.RecoveryReason.IsEmpty()?A.RecoveryReason:A.Reason):M.StopDescription();
 bool Error=A.Stage==EProcessAutoStage::Failed||M.StopStage==EProcessStopStage::Failed;
 if(A.RecoveryPending&&A.RecoveryReason.Contains(TEXT("차단"))){Title=TEXT("배치 재개 차단");Error=true;}
 if(M.StopStage==EProcessStopStage::Failed){Title=TEXT("전체 정지 실패 · 재시도 필요");Why=M.StopDescription()+TEXT(" · ")+M.StopFailureEquipment;if(const auto* Stop=M.History.Find(M.StopRequest))Why+=TEXT(" · ")+Stop->Message;}
 else if(M.IsStopping()){Title=TEXT("전체 정지 확인 중");Why=TEXT("미확인: ")+M.UnconfirmedEquipment();}
 // Keep actionable stop/recovery failures visible even if the same snapshot is also stale or disconnected.
 const bool ProtectedReason=Error||(Same&&A.RecoveryPending&&!A.RecoveryReason.IsEmpty());
 FString DataTitle,DataWhy;
 if(!M.Connected){DataTitle=TEXT("해석 연결 없음");DataWhy=TEXT("현재값 미확인 · 연결 상태를 확인하세요.");}
 else if(!Snap.Error.IsEmpty()){DataTitle=TEXT("작업자 오류");DataWhy=Snap.Error;}
 else if(!Fresh){DataTitle=TEXT("데이터 갱신 확인 필요");DataWhy=TEXT("현재값 미확인 · 이전 수치를 현재값으로 사용하지 않습니다.");}
 if(!DataTitle.IsEmpty()){if(ProtectedReason)Why+=TEXT(" · ")+DataTitle+TEXT(": ")+DataWhy;else{Title=DataTitle;Why=DataWhy;}Error=true;}
 if(Complete&&!Error&&!M.IsStopping())Why=TEXT("확정 원료 전량 도착 · 혼합 시간 충족 · 계산 적용 RPM 0 확인");
 if(A.Active&&A.AdditiveScheduled&&!A.AdditiveDelivered&&A.AdditiveDueSeconds>=0&&A.AdditiveOpenAppliedSeconds<0)
  Why=FString::Chr(TCHAR('A'+A.AdditiveSupplier))+TEXT(" 투입까지 ")+ProcessDisplay::Number(FMath::Max(0.,A.AdditiveDueSeconds-Snap.PhysicalSeconds))+TEXT("초 · ")+Why;
 Put(W,TEXT("OperatorStage"),Title);Put(W,TEXT("OperatorReason"),ProcessDisplay::CompactText(Why));
 if(auto* P=Cast<UBorder>(W->GetWidgetFromName(TEXT("OperatorStatePanel"))))P->SetBrushColor(Error?FLinearColor(1,.86f,.85f):Complete?FLinearColor(.80f,.94f,.85f):FLinearColor(.86f,.93f,.98f));
 if(auto* T=Cast<UTextBlock>(W->GetWidgetFromName(TEXT("OperatorStage"))))T->SetColorAndOpacity(Error?Red:Complete?FLinearColor(.02f,.32f,.14f):Blue);
 FString Summary;
 if(Complete&&!A.Entries.IsEmpty())Summary=TEXT("배치 완료\n최종 혼합기 도착량\n")+ProcessDisplay::CompactText(A.FinalSummary());
 else if(Complete){
  const double AKg=A.BatchMode==EProcessBatchMode::SequentialAB?A.FinalAKg:A.BatchKg;
  Summary=TEXT("배치 완료\n최종 혼합기 도착량\nA  ")+ProcessDisplay::Mass(AKg);
  if(A.BatchMode==EProcessBatchMode::SequentialAB)Summary+=TEXT("\nB  ")+ProcessDisplay::Mass(A.FinalBKg);
  if(A.AdditiveEnabled)Summary+=TEXT("\nC  ")+ProcessDisplay::Mass(A.FinalAdditiveKg);
  Summary+=TEXT("\n합계  ")+ProcessDisplay::Mass(AKg+(A.BatchMode==EProcessBatchMode::SequentialAB?A.FinalBKg:0)+(A.AdditiveEnabled?A.FinalAdditiveKg:0));
 }else{
  auto Mass=[&](const FPlantMassValue& V){return ProcessDisplay::Mass(V.Kg,Fresh&&V.Validity==EPlantValueValidity::Valid);};
  Summary=TEXT("칭량호퍼 현재량  ")+Mass(Snap.WeighingMass)+TEXT("\n배관 이동 중  ")+Mass(Snap.PipeMass)+TEXT("\n수취호퍼에 남은 양  ")+Mass(Snap.ReceiverMass);
  Summary+=TEXT("\n혼합기 현재량  ")+ProcessDisplay::Mass(Snap.InMixer*Snap.ModelParticleMassKg,Fresh&&Snap.ModelParticleMassKg>0);

 }
 Summary+=TEXT("\n계산 적용 RPM  ")+(Fresh&&Snap.AppliedRPMValid?ProcessDisplay::Number(Snap.AppliedRPM):FString(TEXT("미확인")));
 if(A.MixingDurationSeconds>0&&Same)Summary+=TEXT("\n혼합  ")+ProcessDisplay::Number(A.MixingElapsedSeconds)+TEXT(" / ")+ProcessDisplay::Number(A.MixingDurationSeconds)+TEXT("초");
 Summary+=TEXT("\n시뮬레이션 계산값 · 모델 기준");Put(W,TEXT("OperatorSummary"),Summary);
 // Retain unrounded diagnostic evidence in a separate explicit view.
 FString Exact=FString::Printf(TEXT("정밀 수치 · 스냅샷 %lld / 초기화 %lld\n해석 시간 %.17g초\n칭량 %.17g kg / 배관 %.17g kg / 수취 %.17g kg\n요청 압력 [%.17g, %.17g, %.17g] Pa(g)\n적용 압력 [%.17g, %.17g, %.17g] Pa(g)\n목표 RPM %.17g / 계산 적용 RPM %.17g\n\n현재 진단: %s\n작업자: %s\n\n"),Snap.Sequence,Snap.Epoch,Snap.PhysicalSeconds,Snap.WeighingMass.Kg,Snap.PipeMass.Kg,Snap.ReceiverMass.Kg,Snap.RequestedGaugePressurePa.X,Snap.RequestedGaugePressurePa.Y,Snap.RequestedGaugePressurePa.Z,Snap.AppliedGaugePressurePa.X,Snap.AppliedGaugePressurePa.Y,Snap.AppliedGaugePressurePa.Z,Snap.RequestedRPM,Snap.AppliedRPM,*Why,*Snap.Error);
 Exact+=TEXT("적용 레시피 원본 설정\n")+ProcessRecipe::Encode(R)+TEXT("\n\n단계별 진단 / 이력\n");
 if(auto* T=Cast<UTextBlock>(W->GetWidgetFromName(TEXT("PurgeSummary"))))Exact+=TEXT("\n선택 공급 공기 정밀 수치\n")+T->GetText().ToString();
 Put(W,TEXT("OperatorExactValues"),Exact);
 const bool SupplierSelected=ProcessPlantBinding::SupplierIndex(Selected())>=0;
 Show(TEXT("SelectedOverview"),SupplierSelected);
 if(SupplierSelected){
  const auto& P=PlantPurgeFor(Snap,ProcessPlantBinding::SupplierIndex(Selected()));
  const int32 Supplier=ProcessPlantBinding::SupplierIndex(Selected());
  const bool HasBatch=Same&&A.RunId.IsValid();
  const auto* Plan=HasBatch?A.EntryFor(Supplier):R.Entries.FindByPredicate([Supplier](const auto& E){return E.Enabled&&E.Supplier==Supplier;});
  FString Dose;
  const bool Legacy=HasBatch?A.Entries.IsEmpty():R.Entries.IsEmpty();
  const bool LegacyUsed=Legacy&&(HasBatch?A.UsesSupplier(Supplier):
    (S->RecipeRevision>0&&(Supplier==0||(Supplier==1&&R.Mode==EProcessBatchMode::SequentialAB)||(Supplier==2&&R.AdditiveEnabled))));
  if(Plan||LegacyUsed){
   const auto* Mat=Fresh&&HasBatch&&Snap.SequentialBatch.Materials.IsValidIndex(Supplier)?&Snap.SequentialBatch.Materials[Supplier]:nullptr;
   const FString Name=Plan?Plan->DisplayName:Supplier==0?R.MaterialA:Supplier==1?R.MaterialB:R.MaterialC;
   const bool Additive=Plan?Plan->Additive:Supplier==2;
   const bool Full=LegacyUsed&&(HasBatch?A.BatchMode:R.Mode)==EProcessBatchMode::Full;
   const double Target=Plan?Plan->TargetKg:HasBatch?A.DoseFor(Supplier,false):Supplier==0?R.TargetAKg:Supplier==1?R.TargetBKg:R.TargetCKg;
   Dose=FString::Printf(TEXT("%c %s · %s · %s\n목표 %s · 확정 %s"),'A'+Supplier,*Name,Additive?TEXT("첨가제"):TEXT("일반 원료"),*Title,
    Full?TEXT("전량"):*ProcessDisplay::Mass(Target),Mat&&Mat->FinalSimulationSeconds>=0?*ProcessDisplay::Mass(Mat->FinalKg):TEXT("미확인"));
  }
  Put(W,TEXT("SelectedOverview"),Dose.IsEmpty()?FString::Printf(TEXT("공급 %c · 이번 레시피의 계량 설정 없음"),'A'+Supplier):Dose.TrimEnd());
  if(Fresh&&P.Valid)Put(W,TEXT("PurgeSummary"),TEXT("공기 적용 ")+FString(P.AppliedEnabled?TEXT("켜짐"):TEXT("꺼짐"))+TEXT(" · 적용 압력 설정 ")+ProcessDisplay::Number(P.AppliedGaugePa/1000)+TEXT(" kPa(g)\n연결부 남은 양 ")+ProcessDisplay::Mass(P.ResidualKg,true,P.ResidualCount));
 }
 for(const TCHAR* N:{TEXT("PressureSummary"),TEXT("PressureCommandStatus"),TEXT("PurgeSummary"),TEXT("PurgeCommandStatus"),TEXT("AutoDoseSummary"),TEXT("AutoMixTimeSummary"),TEXT("AdditiveSummary"),TEXT("RecipeAppliedSummary"),TEXT("StatusValue"),TEXT("QuantityValue"),TEXT("ValveValue"),TEXT("Disconnected"),TEXT("ThreeDNotice")}){
  if(auto* T=Cast<UTextBlock>(W->GetWidgetFromName(N))){const FString Current=T->GetText().ToString();if(LastCompactText.FindRef(N)!=Current){OriginalDisplayText.Add(N,Current);const FString Short=ProcessDisplay::CompactText(Current);T->SetText(FText::FromString(Short));LastCompactText.Add(N,Short);}T->SetToolTipText(FText::FromString(OriginalDisplayText.FindRef(N)));Font(T,21);}
 }
 for(int I=0;I<9;++I){
  const FName N(*FString::Printf(TEXT("Equipment%dText"),I));auto* T=Cast<UTextBlock>(W->GetWidgetFromName(N));if(!T)continue;
  FString Caption;
  if(I<6){Caption=Fresh?(Snap.AppliedSupplierMask&(1<<I)?TEXT("열림"):TEXT("닫힘")):TEXT("미확인");
   Put(W,*FString::Printf(TEXT("Name%d"),I),FString::Printf(TEXT("공급 %c"),'A'+I));}
  else if(I==6)Caption=TEXT("현재량\n")+ProcessDisplay::Mass(Snap.WeighingMass.Kg,Fresh&&Snap.WeighingMass.Validity==EPlantValueValidity::Valid)+TEXT("\n")+(Fresh?(Snap.AppliedHopperOpen?TEXT("열림"):TEXT("닫힘")):TEXT("미확인"));
  else if(I==7)Caption=TEXT("현재량\n")+ProcessDisplay::Mass(Snap.ReceiverMass.Kg,Fresh&&Snap.ReceiverMass.Validity==EPlantValueValidity::Valid)+TEXT("\n")+(Fresh?(Snap.AppliedReceiverOpen?TEXT("열림"):TEXT("닫힘")):TEXT("미확인"));
  else Caption=(Fresh&&Snap.AppliedRPMValid?ProcessDisplay::Number(Snap.AppliedRPM):TEXT("미확인"))+TEXT(" RPM");
  if(Fresh&&Snap.ValveActuators.IsValidIndex(I))Caption=PlantValveStatus(Snap.ValveActuators[I]);
  T->SetText(FText::FromString(Caption));Font(T,18);T->SetAutoWrapText(false);
  if(auto* B=Cast<UButton>(T->GetParent())){auto Style=B->GetStyle();Style.NormalPadding=Style.PressedPadding=FMargin(4);B->SetStyle(Style);}
 }
 Font(Cast<UTextBlock>(W->GetWidgetFromName(TEXT("SupplyValvePressureSummary"))),21);
 RefreshOperationGuidance();
 // Always visible after operator guidance has selected the current stage, including 3D.
 if(auto* T=Cast<UTextBlock>(W->GetWidgetFromName(TEXT("OperatorReason"))))T->SetText(FText::FromString((S->WebOwnsControl()?S->LocalOperatorReason():FString(TEXT("현재 조작 주체: 언리얼 · 전체 정지는 양쪽에서 가능")))+TEXT("\n")+T->GetText().ToString()));
 if(S->WebOwnsControl())for(const TCHAR* Name:{TEXT("AutoStartA"),TEXT("ResumeBatch"),TEXT("ValveOpen"),TEXT("ValveClose"),TEXT("MixerStart"),TEXT("MixerStop"),TEXT("SetRPM"),TEXT("ManualResume"),TEXT("ManualPause"),TEXT("ApplyPressure"),TEXT("PurgeApplyPressure"),TEXT("PurgeStart"),TEXT("PurgeStop")})if(auto* B=Cast<UButton>(W->GetWidgetFromName(Name))){B->SetIsEnabled(false);B->SetToolTipText(FText::FromString(S->LocalOperatorReason()));}
 for(auto F:NumericFields){
  const FString N=F->Name;double Applied=0;bool Known=true;FString Cross;
  if(N.StartsWith(TEXT("FeedSlot"))){
   const int Slot=N[7]-TCHAR('0');
   Known=R.Entries.IsValidIndex(Slot);
   if(Known)Applied=1000*(N.EndsWith(TEXT("Target"))?R.Entries[Slot].TargetKg:R.Entries[Slot].ToleranceKg);
  }else if(N.StartsWith(TEXT("RecipeSourcePressure"))){
   const int Supplier=N[N.Len()-1]-TCHAR('0');
   Applied=(R.SupplierFeedPa.IsValidIndex(Supplier)?R.SupplierFeedPa[Supplier]:R.FeedPressurePa.X)/1000.;
  }else if(N==TEXT("RecipeTargetA")||N==TEXT("TargetMassInput"))Applied=R.TargetAKg*1000;
  else if(N==TEXT("RecipeToleranceA")||N==TEXT("WeighToleranceInput"))Applied=R.ToleranceAKg*1000;
  else if(N==TEXT("RecipeTargetB")||N==TEXT("BTargetMassInput"))Applied=R.TargetBKg*1000;
  else if(N==TEXT("RecipeToleranceB")||N==TEXT("BWeighToleranceInput"))Applied=R.ToleranceBKg*1000;
  else if(N==TEXT("RecipeTargetC")||N==TEXT("AdditiveTargetInput"))Applied=R.TargetCKg*1000;
  else if(N==TEXT("RecipeToleranceC")||N==TEXT("AdditiveToleranceInput"))Applied=R.ToleranceCKg*1000;
  else if(N==TEXT("RecipeReceiverPressure"))Applied=R.TransferPressurePa.Z/1000.;
  else if(N==TEXT("RecipeRPM"))Applied=R.TargetRPM;
  else if(N==TEXT("RPMInput")){
   Applied=A.Active?Snap.AppliedRPM:Snap.RequestedRPM;Known=Fresh&&(!A.Active||Snap.AppliedRPMValid);
   double Draft=0;
   if(A.Active&&ProcessDisplay::Parse(F->Input->GetText().ToString(),Draft)&&Draft==0)
    Cross=TEXT("정지하려면 전체 정지를 사용하세요. 운전 중에는 양수 RPM을 입력하세요.");
  }
  else if(N==TEXT("RecipeMixSeconds")||N==TEXT("AutoMixTimeInput"))Applied=R.MixingSeconds;
  else if(N==TEXT("RecipeDelay")||N==TEXT("AdditiveDelayInput"))Applied=R.DelaySeconds;
  else if(N==TEXT("PressureInput")){const int32 Target=PlantControl::PressureTarget(Selected());Known=Fresh&&Target!=INDEX_NONE&&Snap.AppliedPressureValid;Applied=Known?(Target==0?Snap.SupplierPressure(ProcessPlantBinding::SupplierIndex(Selected()),true):Snap.AppliedGaugePressurePa[Target])/1000:0;F->Maximum=Target==2?0:1;}
  else if(N==TEXT("PurgePressureInput")&&ProcessPlantBinding::SupplierIndex(Selected())>=0){const auto& P=PlantPurgeFor(Snap,ProcessPlantBinding::SupplierIndex(Selected()));Applied=P.AppliedGaugePa/1000;Known=Fresh&&P.Valid;}
  auto Compare=[&](const FString& Other,bool Less){if(auto* X=Cast<UEditableTextBox>(W->GetWidgetFromName(FName(*Other)))){double V=0,T=0;if(ProcessDisplay::Parse(F->Input->GetText().ToString(),V)&&ProcessDisplay::Parse(X->GetText().ToString(),T)&&V>=T)Cross=Less?TEXT("계량 오차는 목표량보다 작아야 합니다."):TEXT("예약 시각은 혼합 종료보다 앞서야 합니다.");}};
  if(N.StartsWith(TEXT("RecipeTolerance")))Compare(N.Replace(TEXT("Tolerance"),TEXT("Target")),true);
  if(N==TEXT("RecipeDelay"))Compare(TEXT("RecipeMixSeconds"),false);
  F->Refresh(Applied,Known,Cross);
 }
 RefreshResponsiveOperatorLayout();
}
