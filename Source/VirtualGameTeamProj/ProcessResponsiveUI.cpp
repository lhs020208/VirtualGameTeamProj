#include "ProcessControlUI.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"

// All measurements are in UMG layout units, after the viewport DPI scale. Keep
// the existing widgets/delegates: resizing and view switching never issue commands.
void UProcessControlUI::BuildResponsiveOperatorLayout(UCanvasPanel* Root)
{
    auto* W=Widget.Get();auto* Tree=W->WidgetTree.Get();
    auto Get=[&](const TCHAR* Name){return W->GetWidgetFromName(Name);};
    auto* Header=CastChecked<UBorder>(Get(TEXT("Header")));
    auto* State=CastChecked<UBorder>(Get(TEXT("OperatorStatePanel")));
    auto* Body=CastChecked<UBorder>(Get(TEXT("BodyBackground")));
    auto* Footer=CastChecked<UBorder>(Get(TEXT("OperatorActionPanel")));
    auto* Notice=Get(TEXT("ThreeDNotice"));
    auto* Frame=Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("OperatorResponsiveFrame"));
    auto* FrameSlot=Root->AddChildToCanvas(Frame);
    FrameSlot->SetAnchors(FAnchors(0,0,1,1));FrameSlot->SetOffsets(FMargin(12));FrameSlot->SetZOrder(100);
    Frame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    // Auto-height bands reserve their actual space, including wrapped Korean text.
    Header->RemoveFromParent();Frame->AddChildToVerticalBox(Header);
    const auto Navigation=CastChecked<UHorizontalBox>(Header->GetContent())->GetAllChildren();
    auto* Nav=Tree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(),TEXT("OperatorNavigationWrap"));
    Nav->SetExplicitWrapSize(true);Nav->SetInnerSlotPadding(FVector2D(8,6));
    Header->SetContent(Nav);Header->SetPadding(FMargin(12,8));
    for(auto* Item:Navigation){Item->RemoveFromParent();Nav->AddChildToWrapBox(Item)->SetVerticalAlignment(VAlign_Center);}

    auto* StateContent=State->GetContent();StateContent->RemoveFromParent();
    auto* StateScroll=Tree->ConstructWidget<UScrollBox>();StateScroll->AddChild(StateContent);
    auto* StateLimit=Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("OperatorStateLimit"));
    StateLimit->SetMaxDesiredHeight(180);StateLimit->SetContent(StateScroll);State->SetContent(StateLimit);
    State->SetPadding(FMargin(12,8));State->RemoveFromParent();Frame->AddChildToVerticalBox(State)->SetPadding(FMargin(0,8));

    auto* Main=Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass(),TEXT("OperatorViewportArea"));
    Main->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Frame->AddChildToVerticalBox(Main)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto FillMain=[&](UWidget* Panel){
        Panel->RemoveFromParent();auto* Slot=Main->AddChildToOverlay(Panel);
        Slot->SetHorizontalAlignment(HAlign_Fill);Slot->SetVerticalAlignment(VAlign_Fill);
        Panel->SetClipping(EWidgetClipping::ClipToBounds);
    };
    FillMain(Body);
    auto* Columns=CastChecked<UHorizontalBox>(Body->GetContent());Columns->RemoveFromParent();
    // Retrieve from the detached columns, which are temporarily outside the widget tree.
    auto* Diagram=Columns->GetChildAt(0);auto* Side=Columns->GetChildAt(1);Columns->ClearChildren();
    auto* WideDiagram=Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("OperatorWideDiagram"));WideDiagram->SetContent(Diagram);
    FSlateChildSize Left(ESlateSizeRule::Fill);Left.Value=1.3f;Columns->AddChildToHorizontalBox(WideDiagram)->SetSize(Left);
    auto* SideSlot=Columns->AddChildToHorizontalBox(Side);SideSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));SideSlot->SetPadding(FMargin(20,0,0,0));
    auto* Layouts=Tree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(),TEXT("OperatorBodyLayouts"));
    Body->SetContent(Layouts);Layouts->AddChild(Columns);
    auto* CompactScroll=Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),TEXT("OperatorCompactScroll"));Layouts->AddChild(CompactScroll);
    auto* Compact=Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("OperatorStackedBody"));CompactScroll->AddChild(Compact);
    Compact->AddChildToVerticalBox(Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("OperatorCompactDiagram")));

    // The observation hint has its own contrasting background and does not
    // occupy the same canvas coordinates as the operator status banner.
    Notice->RemoveFromParent();
    auto* NoticeScroll=Tree->ConstructWidget<UScrollBox>();NoticeScroll->AddChild(Notice);
    auto* NoticeLimit=Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("OperatorObservationLimit"));
    NoticeLimit->SetContent(NoticeScroll);
    auto* NoticePanel=Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("OperatorObservationPanel"));
    NoticePanel->SetBrushColor(FLinearColor(.025f,.055f,.085f,.95f));NoticePanel->SetPadding(FMargin(12,8));NoticePanel->SetContent(NoticeLimit);
    auto* NS=Main->AddChildToOverlay(NoticePanel);NS->SetHorizontalAlignment(HAlign_Left);NS->SetVerticalAlignment(VAlign_Top);

    FillMain(RecipePanel);
    FillMain(OperatorDetails);

    auto* Hint=Get(TEXT("OperatorActionHint"));Hint->RemoveFromParent();
    Footer->RemoveFromParent();Frame->AddChildToVerticalBox(Footer)->SetPadding(FMargin(0,8,0,0));
    auto* FooterBox=Tree->ConstructWidget<UVerticalBox>();Footer->SetContent(FooterBox);Footer->SetPadding(FMargin(12,8));
    auto* HintScroll=Tree->ConstructWidget<UScrollBox>();HintScroll->AddChild(Hint);
    auto* HintLimit=Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("OperatorActionHintLimit"));
    HintLimit->SetContent(HintScroll);FooterBox->AddChildToVerticalBox(HintLimit)->SetPadding(FMargin(0,0,0,6));
    FooterBox->AddChildToVerticalBox(GlobalControls);
    for(auto* Item:GlobalControls->GetAllChildren()){
        if(auto* Box=Cast<USizeBox>(Item))Box->ClearWidthOverride();
        auto* Slot=CastChecked<UHorizontalBoxSlot>(Item->Slot);Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Slot->SetPadding(FMargin(4,0));
    }

    // Keep only the recipe title and Apply/Return actions outside its scroll.
    // Long applied summaries and validation messages must not consume the editor.
    auto* RecipeScroll=CastChecked<UScrollBox>(Get(TEXT("RecipeScroll")));
    auto* Settings=CastChecked<UVerticalBox>(RecipeScroll->GetChildAt(0));
    const auto Fields=Settings->GetAllChildren();Settings->ClearChildren();
    for(const TCHAR* Name:{TEXT("RecipeNotice"),TEXT("OperatorAppliedRecipe"),TEXT("RecipeMessage")}){
        auto* Item=Get(Name);Item->RemoveFromParent();Settings->AddChildToVerticalBox(Item)->SetPadding(FMargin(0,4));
    }
    for(auto* Item:Fields)Settings->AddChildToVerticalBox(Item)->SetPadding(FMargin(2,5));

    // Reflow rows of recipe fields/buttons, retaining the named outer rows used
    // for mode visibility. Numeric input +/-/unit rows remain intact.
    TArray<UWidget*> All;Tree->GetAllWidgets(All);
    for(auto* Item:All){
        if(auto* Button=Cast<UButton>(Item)){
            if(auto* Label=Cast<UTextBlock>(Button->GetContent())){
                // Center the text within the available width, not a stale desired
                // width from the previously wrapped label after a viewport resize.
                if(auto* Slot=Cast<UButtonSlot>(Label->Slot))Slot->SetHorizontalAlignment(HAlign_Fill);
                Label->SetJustification(ETextJustify::Center);
            }
            bool InRecipe=false;for(auto* P=Button->GetParent();P;P=P->GetParent())InRecipe|=P==RecipePanel;
            if(InRecipe)if(auto* T=Cast<UTextBlock>(Button->GetContent())){
                auto F=T->GetFont();F.Size=18;T->SetFont(F);T->SetAutoWrapText(true);T->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
            }
        }
        auto* Row=Cast<UHorizontalBox>(Item);if(!Row||Row->GetChildrenCount()<2)continue;
        bool InRecipe=false,InNumeric=false;
        for(auto* P=Row->GetParent();P;P=P->GetParent()){
            InRecipe|=P==RecipePanel;InNumeric|=P->GetName().EndsWith(TEXT("Field"));
        }
        if(!InRecipe||InNumeric||Row==Get(TEXT("RecipeTitle"))->GetParent())continue;
        const auto Cells=Row->GetAllChildren();Row->ClearChildren();
        auto* Wrap=Tree->ConstructWidget<UWrapBox>();Wrap->SetExplicitWrapSize(true);Wrap->SetInnerSlotPadding(FVector2D(10,8));
        Row->AddChildToHorizontalBox(Wrap)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        for(auto* Cell:Cells){auto* Box=Tree->ConstructWidget<USizeBox>();Box->SetContent(Cell);Wrap->AddChildToWrapBox(Box);}
        ResponsiveRecipeRows.Add(Wrap);
    }
}

void UProcessControlUI::RefreshResponsiveOperatorLayout()
{
    auto* W=Widget.Get();if(!W)return;
    auto Get=[&](const TCHAR* Name){return W->GetWidgetFromName(Name);};
    FVector2D Size=W->GetCachedGeometry().GetLocalSize();
    if(Size.X<=0||Size.Y<=0)Size=FVector2D(1280,900);
    const float Width=FMath::Max(240.f,float(Size.X)-24.f);
    const bool Stacked=Width<1150;
    CastChecked<UWrapBox>(Get(TEXT("OperatorNavigationWrap")))->SetWrapSize(Width-24);
    CastChecked<USizeBox>(Get(TEXT("OperatorStateLimit")))->SetMaxDesiredHeight(FMath::Clamp(float(Size.Y)*.22f,64.f,200.f));
    CastChecked<USizeBox>(Get(TEXT("OperatorActionHintLimit")))->SetMaxDesiredHeight(FMath::Clamp(float(Size.Y)*.10f,36.f,90.f));
    auto* NoticeLimit=CastChecked<USizeBox>(Get(TEXT("OperatorObservationLimit")));
    NoticeLimit->SetWidthOverride(FMath::Min(Width-24,660.f));NoticeLimit->SetMaxDesiredHeight(FMath::Clamp(float(Size.Y)*.18f,54.f,160.f));
    Get(TEXT("OperatorObservationPanel"))->SetVisibility(Is3D()&&!RecipeOpen&&!OperatorDetailsOpen?ESlateVisibility::Visible:ESlateVisibility::Collapsed);

    auto* Layouts=CastChecked<UWidgetSwitcher>(Get(TEXT("OperatorBodyLayouts")));
    CastChecked<USizeBox>(Get(TEXT("OperatorCompactDiagram")))->SetHeightOverride((Width-60)*730.f/1100.f);
    if(Layouts->GetActiveWidgetIndex()!=(Stacked?1:0)){
        auto* Diagram=Get(TEXT("OperatorDiagramScale"));auto* Right=Get(TEXT("OperatorSideContent"));
        Diagram->RemoveFromParent();Right->RemoveFromParent();
        if(Stacked){
            CastChecked<USizeBox>(Get(TEXT("OperatorCompactDiagram")))->SetContent(Diagram);
            CastChecked<UVerticalBox>(Get(TEXT("OperatorStackedBody")))->AddChildToVerticalBox(Right)->SetPadding(FMargin(0,12,0,0));
        }else{
            CastChecked<USizeBox>(Get(TEXT("OperatorWideDiagram")))->SetContent(Diagram);
            CastChecked<UScrollBox>(Get(TEXT("OperatorSideScroll")))->AddChild(Right);
        }
        Layouts->SetActiveWidgetIndex(Stacked?1:0);
    }

    // Normalize after legacy refresh routines have styled labels. Wrapping uses
    // allotted widths; never impose a font-height-sized box on Korean glyphs.
    auto Font=[&](const TCHAR* Name,int32 Points,FLinearColor Color){if(auto* T=Cast<UTextBlock>(Get(Name))){auto F=T->GetFont();F.Size=Points;T->SetFont(F);T->SetColorAndOpacity(Color);T->SetAutoWrapText(true);T->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);}};
    const FLinearColor Ink(.035f,.075f,.12f);
    Font(TEXT("Title"),22,FLinearColor::White);
    CastChecked<UTextBlock>(Get(TEXT("Title")))->SetAutoWrapText(false);
    Font(TEXT("OperatorReason"),17,Ink);Font(TEXT("OperatorSummary"),20,Ink);Font(TEXT("SelectedOverview"),20,Ink);
    Font(TEXT("OperatorActionHint"),16,FLinearColor::White);Font(TEXT("ThreeDNotice"),17,FLinearColor::White);
    for(const TCHAR* Name:{TEXT("View2D"),TEXT("View3D"),TEXT("RecipeToggle"),TEXT("OperatorDetailsToggle"),TEXT("AutoStartA"),TEXT("ResumeBatch"),TEXT("GlobalStop"),TEXT("ManualResume"),TEXT("ManualPause"),TEXT("OperatorOpenRecipe"),TEXT("CheckBatch")}){
        if(auto* B=Cast<UButton>(Get(Name))){auto S=B->GetStyle();S.NormalPadding=S.PressedPadding=FMargin(10,8);B->SetStyle(S);
            if(auto* T=Cast<UTextBlock>(B->GetContent())){auto F=T->GetFont();F.Size=18;T->SetFont(F);T->SetAutoWrapText(!B->GetName().StartsWith(TEXT("View"))&&B->GetName()!=TEXT("RecipeToggle")&&B->GetName()!=TEXT("OperatorDetailsToggle"));T->SetWrapTextAt(0);T->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);}
        }
    }
    const float RecipeWidth=FMath::Max(200.f,Width-60.f);
    for(auto Row:ResponsiveRecipeRows){
        Row->SetWrapSize(RecipeWidth);
        const int32 Columns=FMath::Clamp(FMath::FloorToInt((RecipeWidth+10)/390.f),1,FMath::Min(3,Row->GetChildrenCount()));
        const float CellWidth=(RecipeWidth-(Columns-1)*10)/Columns;
        for(auto* Cell:Row->GetAllChildren())CastChecked<USizeBox>(Cell)->SetWidthOverride(CellWidth);
    }
}
