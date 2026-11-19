#include "ProcessNumericField.h"
#include "ProcessDisplay.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Brushes/SlateRoundedBoxBrush.h"

bool UProcessNumericField::Attach(UUserWidget* W,const TCHAR* N,const FString& U,double S,double Min,double Max,bool Pos)
{
 Input=Cast<UEditableTextBox>(W->GetWidgetFromName(N));if(!Input||!Input->GetParent())return false;
 Name=N;Unit=U;Step=S;Minimum=Min;Maximum=Max;Positive=Pos;
 auto* Tree=W->WidgetTree.Get();auto* Parent=Input->GetParent();const int32 Index=Parent->GetChildIndex(Input);
 Frame=Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),FName(*(Name+TEXT("Field"))));
 const auto Children=Parent->GetAllChildren();TArray<UPanelSlot*> Templates;
 for(auto* Child:Children)Templates.Add(Child->Slot);
 Parent->ClearChildren();
 for(int32 I=0;I<Children.Num();++I)Parent->AddChild(I==Index?static_cast<UWidget*>(Frame):Children[I],Templates[I]);
 auto* Row=Tree->ConstructWidget<UHorizontalBox>();Frame->AddChildToVerticalBox(Row);
 auto Text=[&](const FString& V,int Size){auto* T=Tree->ConstructWidget<UTextBlock>();auto F=T->GetFont();F.Size=Size;T->SetFont(F);T->SetText(FText::FromString(V));T->SetColorAndOpacity(FLinearColor(.04f,.09f,.15f));return T;};
 auto MakeButton=[&](const TCHAR* Suffix,const TCHAR* Caption){auto* B=Tree->ConstructWidget<UButton>(UButton::StaticClass(),FName(*(Name+Suffix)));
  ProcessDisplay::MouseOnly(B);B->AddChild(Text(Caption,26));auto Style=B->GetStyle();Style.Normal=FSlateRoundedBoxBrush(FLinearColor(.83f,.90f,.95f),6.f);Style.Hovered=FSlateRoundedBoxBrush(FLinearColor(.70f,.83f,.92f),6.f);Style.Pressed=FSlateRoundedBoxBrush(FLinearColor(.58f,.75f,.87f),6.f);Style.NormalPadding=Style.PressedPadding=FMargin(12,8);B->SetStyle(Style);
  auto* Box=Tree->ConstructWidget<USizeBox>();Box->SetWidthOverride(54);Box->SetMinDesiredHeight(58);Box->SetContent(B);Row->AddChildToHorizontalBox(Box)->SetVerticalAlignment(VAlign_Center);return B;};
 Minus=MakeButton(TEXT("Minus"),TEXT("−"));Minus->OnClicked.AddDynamic(this,&UProcessNumericField::Decrease);
 auto* Size=Tree->ConstructWidget<USizeBox>();Size->SetMinDesiredHeight(58);Size->SetContent(Input);
 auto* Slot=Row->AddChildToHorizontalBox(Size);Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Slot->SetPadding(FMargin(6,0));
 auto Style=Input->GetWidgetStyle();auto Font=Style.TextStyle.Font;Font.Size=23;Style.SetFont(Font);Style.Padding=FMargin(10,9);Style.SetForegroundColor(FLinearColor(.04f,.09f,.15f));Input->SetWidgetStyle(Style);
 Input->SetClearKeyboardFocusOnCommit(false);Input->SetSelectAllTextOnCommit(false);Input->SetSelectAllTextWhenFocused(false);
 Input->OnTextCommitted.Clear(); // Enter commits draft text only; explicit Apply/Start owns the command path.
 Input->OnTextChanged.AddDynamic(this,&UProcessNumericField::Changed);
 Plus=MakeButton(TEXT("Plus"),TEXT("+"));Plus->OnClicked.AddDynamic(this,&UProcessNumericField::Increase);
 auto* UnitBox=Tree->ConstructWidget<USizeBox>();UnitBox->SetWidthOverride(U==TEXT("kPa(g)")?105:70);UnitBox->SetContent(Text(U,20));
 auto* US=Row->AddChildToHorizontalBox(UnitBox);US->SetPadding(FMargin(10,0,0,0));US->SetVerticalAlignment(VAlign_Center);
 Feedback=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),FName(*(Name+TEXT("Feedback"))));auto F=Feedback->GetFont();F.Size=17;Feedback->SetFont(F);Feedback->SetAutoWrapText(true);Frame->AddChildToVerticalBox(Feedback)->SetPadding(FMargin(0,3,0,6));
 Input->SetToolTipText(FText::FromString(TEXT("직접 입력은 원래 정밀도를 유지합니다. +/− 간격 ")+ProcessDisplay::Exact(S)+TEXT(" ")+U+TEXT(". Enter·휠·화면 갱신은 적용하지 않습니다.")));
 return true;
}
void UProcessNumericField::Changed(const FText&){Touched=true;}
void UProcessNumericField::Adjust(double Direction)
{
 if(!Input||!Input->GetIsEnabled())return;
 double V=0;if(!Input->GetText().IsEmpty()&&!ProcessDisplay::Parse(Input->GetText().ToString(),V)){
  Feedback->SetText(FText::FromString(TEXT("숫자를 확인한 뒤 증감하세요.")));return;
 }
 // A decimal button step keeps the decimal precision the user entered; do not expose binary .2+.1 noise.
 auto Places=[](FString Text){Text=Text.TrimStartAndEnd().ToLower();int32 E=INDEX_NONE,Dot=INDEX_NONE,Exponent=0;if(Text.FindChar(TEXT('e'),E)){Exponent=int32(FMath::Clamp(FCString::Atod(*Text.Mid(E+1)),-324.,309.));Text=Text.Left(E);}return Text.FindChar(TEXT('.'),Dot)?FMath::Max(0,Text.Len()-Dot-1-Exponent):FMath::Max(0,-Exponent);};
 const double Next=V+Direction*Step;const int32 Precision=FMath::Clamp(FMath::Max(Places(Input->GetText().ToString()),Places(ProcessDisplay::Exact(Step))),0,17);
 FString Draft=FMath::Abs(Next)<1.e15?FString::Printf(TEXT("%.*f"),Precision,Next):ProcessDisplay::Exact(Next);
 if(FMath::Abs(Next)<1.e15&&Draft.Contains(TEXT("."))){while(Draft.EndsWith(TEXT("0")))Draft.LeftChopInline(1);if(Draft.EndsWith(TEXT(".")))Draft.LeftChopInline(1);}
 Input->SetText(FText::FromString(Draft));Touched=true;
}
void UProcessNumericField::Increase(){Adjust(1);}
void UProcessNumericField::Decrease(){Adjust(-1);}
void UProcessNumericField::Refresh(double Applied,bool HasApplied,const FString& CrossError)
{
 if(!Input||!Frame)return;
 Frame->SetVisibility(Input->GetVisibility());Plus->SetIsEnabled(Input->GetIsEnabled());Minus->SetIsEnabled(Input->GetIsEnabled());
 const FString T=Input->GetText().ToString();double V=0;FString Error;
 if(!ProcessDisplay::Parse(T,V))Error=T.IsEmpty()?TEXT("값을 입력하세요."):TEXT("유한한 숫자를 입력하세요. 단위는 입력하지 않습니다.");
 else if(V<Minimum||V>Maximum||(Positive&&V==0))Error=ProcessDisplay::Number(Minimum)+(Positive?TEXT(" 초과"):TEXT(" 이상"))+(Maximum<1.e10?TEXT(", ")+ProcessDisplay::Number(Maximum)+TEXT(" 이하"):FString())+TEXT("로 입력하세요.");
 else Error=CrossError;
 const bool Diff=HasApplied&&Error.IsEmpty()&&V!=Applied;
 const FString AppliedText=HasApplied?TEXT("적용 ")+ProcessDisplay::Number(Applied)+TEXT(" ")+Unit:TEXT("적용값 미확인");
 const FString Message=(!Error.IsEmpty()&&Touched)?Error:Error.IsEmpty()?(Diff?TEXT("편집값 · 미적용 / ")+AppliedText:TEXT("적용값과 같음")):AppliedText+TEXT(" · 직접 입력 가능");
 Feedback->SetText(FText::FromString(Message));Feedback->SetColorAndOpacity(!Error.IsEmpty()&&Touched?FLinearColor(.70f,.08f,.08f):Diff?FLinearColor(.55f,.30f,.02f):FLinearColor(.21f,.31f,.39f));
 if(!Error.IsEmpty()&&Touched)Input->SetError(FText::FromString(Error));else Input->ClearError();
 // In particular, do not SetText, select text, move the caret, or dispatch anything during Refresh.
}
void UProcessNumericField::Detach(){if(Input)Input->OnTextChanged.RemoveAll(this);if(Plus)Plus->OnClicked.RemoveAll(this);if(Minus)Minus->OnClicked.RemoveAll(this);}
