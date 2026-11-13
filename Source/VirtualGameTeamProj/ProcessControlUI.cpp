#include "ProcessControlUI.h"
#include "ProcessPlantBinding.h"
#include "ProcessControlService.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "HAL/PlatformTime.h"
#include <cstdlib>
#include "UObject/UnrealType.h"
#include <limits>

namespace
{
void Label(UUserWidget* W,const TCHAR* Name,const FString& Text)
{
    if(auto* T=Cast<UTextBlock>(W->GetWidgetFromName(Name)))T->SetText(FText::FromString(Text));
}
void ButtonLabel(UButton* B,const TCHAR* Text)
{
    const FLinearColor Ink(.025f,.055f,.09f);
    if(auto* T=Cast<UTextBlock>(B->GetContent())){T->SetText(FText::FromString(Text));auto Font=T->GetFont();Font.Size=15;T->SetFont(Font);T->SetAutoWrapText(false);T->SetColorAndOpacity(Ink);}
    auto Style=B->GetStyle();
    Style.Normal=FSlateRoundedBoxBrush(FLinearColor(.76f,.88f,.94f),6.f);
    Style.Hovered=FSlateRoundedBoxBrush(FLinearColor(.65f,.83f,.92f),6.f);
    Style.Pressed=FSlateRoundedBoxBrush(FLinearColor(.54f,.74f,.86f),6.f);
    Style.Disabled=FSlateRoundedBoxBrush(FLinearColor(.88f,.90f,.92f),6.f);
    Style.NormalPadding=FMargin(8,7);Style.PressedPadding=FMargin(8,7);B->SetStyle(Style);
}
}

bool UProcessControlUI::Attach(UUserWidget* W,UProcessControlService* S)
{
    Detach();if(!W||!S||!W->WidgetTree)return false;
    auto* Root=Cast<UCanvasPanel>(W->WidgetTree->RootWidget);if(!Root)return false;
    Widget=W;Service=S;
    GlobalControls=W->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),TEXT("ManualControlBar"));
    auto* Slot=Root->AddChildToCanvas(GlobalControls);Slot->SetAnchors(FAnchors(0,0,1,0));Slot->SetOffsets(FMargin(360,12,20,44));Slot->SetZOrder(10);
    for(const auto& Name:{TEXT("GlobalStop"),TEXT("ManualResume"),TEXT("ManualPause")})
    {
        auto* B=W->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),FName(Name));
        auto* T=W->WidgetTree->ConstructWidget<UTextBlock>();B->AddChild(T);
        auto* RowSlot=GlobalControls->AddChildToHorizontalBox(B);RowSlot->SetPadding(FMargin(4,0));RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }
    auto Prepare=[&](const TCHAR* Name,const TCHAR* Text){auto* B=Cast<UButton>(W->GetWidgetFromName(Name));if(B){B->OnClicked.Clear();ButtonLabel(B,Text);}return B;};
    if(auto* B=Prepare(TEXT("ValveOpen"),TEXT("밸브 열기")))B->OnClicked.AddDynamic(this,&UProcessControlUI::OpenValve);
    if(auto* B=Prepare(TEXT("ValveClose"),TEXT("밸브 닫기")))B->OnClicked.AddDynamic(this,&UProcessControlUI::CloseValve);
    if(auto* B=Prepare(TEXT("MixerStart"),TEXT("혼합 시작")))B->OnClicked.AddDynamic(this,&UProcessControlUI::StartMixer);
    if(auto* B=Prepare(TEXT("MixerStop"),TEXT("혼합 정지")))B->OnClicked.AddDynamic(this,&UProcessControlUI::StopMixer);
    if(auto* B=Prepare(TEXT("SetRPM"),TEXT("목표 회전속도 설정")))B->OnClicked.AddDynamic(this,&UProcessControlUI::SetRPM);
    if(auto* B=Prepare(TEXT("ManualRequest"),TEXT("수동 조작 · 전체 정지")))B->OnClicked.AddDynamic(this,&UProcessControlUI::StopAll);
    if(auto* B=Prepare(TEXT("GlobalStop"),TEXT("수동 조작 · 전체 정지")))B->OnClicked.AddDynamic(this,&UProcessControlUI::StopAll);
    if(auto* B=Prepare(TEXT("ManualResume"),TEXT("수동 운전 · 계산 재개")))B->OnClicked.AddDynamic(this,&UProcessControlUI::Resume);
    if(auto* B=Prepare(TEXT("ManualPause"),TEXT("수동 일시정지 · 전체 정지")))B->OnClicked.AddDynamic(this,&UProcessControlUI::Pause);
    if(auto* Input=Cast<UEditableTextBox>(W->GetWidgetFromName(TEXT("RPMInput"))))
    {Input->SetText(FText::AsNumber(S->State.Snapshot.RequestedRPM));Input->SetHintText(FText::FromString(TEXT("0~30 rpm")));}
    if(auto* Help=W->GetWidgetFromName(TEXT("Help")))if(auto* Canvas=Cast<UCanvasPanelSlot>(Help->Slot))
    {Canvas->SetAnchors(FAnchors(0,0,1,0));Canvas->SetOffsets(FMargin(370,112,20,50));}
    // Reuse the existing buttons in compact rows, before the potentially long outcome explanation.
    if(auto* Panel=Cast<UVerticalBox>(W->GetWidgetFromName(TEXT("PanelContents"))))
    {
        TMap<FString,UWidget*> Controls;
        for(const TCHAR* N:{TEXT("ValveOpen"),TEXT("ValveClose"),TEXT("MixerStart"),TEXT("MixerStop"),TEXT("TransferStart"),TEXT("TransferStop"),TEXT("RPMLabel"),TEXT("RPMInput"),TEXT("SetRPM"),TEXT("TransferTelemetry"),TEXT("Disconnected"),TEXT("DataSource"),TEXT("CloseButton")})
            if(auto* C=W->GetWidgetFromName(N)){Controls.Add(N,C);Panel->RemoveChild(C);}
        auto Row=[&](const TCHAR* Name,const TCHAR* Left,const TCHAR* Right){
            auto* R=W->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),FName(Name));
            Panel->AddChildToVerticalBox(R)->SetPadding(FMargin(6,3));
            for(const TCHAR* N:{Left,Right})if(auto* C=Controls.FindRef(N)){auto* CS=R->AddChildToHorizontalBox(C);CS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));CS->SetPadding(FMargin(2,0));}
        };
        Row(TEXT("ManualValveRow"),TEXT("ValveOpen"),TEXT("ValveClose"));Row(TEXT("ManualMixerRow"),TEXT("MixerStart"),TEXT("MixerStop"));
        for(const TCHAR* N:{TEXT("RPMLabel"),TEXT("RPMInput"),TEXT("SetRPM")})if(auto* C=Controls.FindRef(N))Panel->AddChildToVerticalBox(C)->SetPadding(FMargin(6,3));
        BuildPressureControls(Panel);BuildPurgeControls(Panel);
        Row(TEXT("ManualTransferRow"),TEXT("TransferStart"),TEXT("TransferStop"));
        for(const TCHAR* N:{TEXT("TransferTelemetry"),TEXT("Disconnected"),TEXT("DataSource"),TEXT("CloseButton")})if(auto* C=Controls.FindRef(N))Panel->AddChildToVerticalBox(C)->SetPadding(FMargin(6,3));
    }
    Label(W,TEXT("Title"),TEXT("원료 투입 · 혼합\n수동 운전 · 상태 확인"));
    Refresh();return true;
}

void UProcessControlUI::Detach()
{
    if(auto* W=Widget.Get())for(const TCHAR* Name:{TEXT("ValveOpen"),TEXT("ValveClose"),TEXT("MixerStart"),TEXT("MixerStop"),TEXT("SetRPM"),TEXT("ApplyPressure"),TEXT("PurgeApplyPressure"),TEXT("PurgeStart"),TEXT("PurgeStop"),TEXT("ManualRequest"),TEXT("GlobalStop"),TEXT("ManualResume"),TEXT("ManualPause")})
        if(auto* B=Cast<UButton>(W->GetWidgetFromName(Name)))B->OnClicked.RemoveAll(this);
    if(PurgeControls)PurgeControls->RemoveFromParent();PurgeControls=nullptr;PurgeDraftEquipment.Empty();PurgeDraftEpoch=-1;
    if(PressureControls)PressureControls->RemoveFromParent();PressureControls=nullptr;PressureDraftEquipment.Empty();PressureDraftEpoch=-1;
    if(GlobalControls)GlobalControls->RemoveFromParent();GlobalControls=nullptr;Widget.Reset();Service.Reset();
}

FString UProcessControlUI::Selected() const
{
    auto* W=Widget.Get();auto* P=W?FindFProperty<FStrProperty>(W->GetClass(),TEXT("SelectedEquipmentId")):nullptr;
    return P?P->GetPropertyValue_InContainer(W):FString();
}
bool UProcessControlUI::Is3D() const
{
    auto* W=Widget.Get();auto* P=W?FindFProperty<FBoolProperty>(W->GetClass(),TEXT("Is3D")):nullptr;
    return P&&P->GetPropertyValue_InContainer(W);
}
void UProcessControlUI::Send(EProcessCommand C,double V,bool Global)
{
    if(Is3D())return; // Observation, selection and hover never issue commands.
    LocalError.Empty();if(auto* S=Service.Get())S->Request(Global?TEXT("plant"):Selected(),C,V);Refresh();
}
void UProcessControlUI::OpenValve(){Send(EProcessCommand::Valve,1);}
void UProcessControlUI::CloseValve(){Send(EProcessCommand::Valve,0);}
void UProcessControlUI::StartMixer(){Send(EProcessCommand::MixerEnabled,1);}
void UProcessControlUI::StopMixer(){Send(EProcessCommand::MixerEnabled,0);}
void UProcessControlUI::StopAll(){Send(EProcessCommand::StopAll,0,true);}
void UProcessControlUI::Resume(){Send(EProcessCommand::Resume,0,true);}
void UProcessControlUI::Pause(){Send(EProcessCommand::Pause,0,true);}
void UProcessControlUI::SetRPM()
{
    auto* W=Widget.Get();auto* Input=W?Cast<UEditableTextBox>(W->GetWidgetFromName(TEXT("RPMInput"))):nullptr;
    double RPM=std::numeric_limits<double>::quiet_NaN();
    if(Input){FString Text=Input->GetText().ToString().TrimStartAndEnd();FTCHARToUTF8 Bytes(*Text);char* End=nullptr;RPM=std::strtod(Bytes.Get(),&End);if(Text.IsEmpty()||End==Bytes.Get()||!End||*End!=0)RPM=std::numeric_limits<double>::quiet_NaN();}
    Send(EProcessCommand::MixerRPM,RPM);
}

void UProcessControlUI::Refresh()
{
    auto* W=Widget.Get();auto* S=Service.Get();if(!W||!S)return;
    const auto& State=S->State;const double Now=FPlatformTime::Seconds();const FString Id=Selected();const bool Observation=Is3D(),Mixer=Id==TEXT("mixer");
    if(GlobalControls)GlobalControls->SetVisibility(Observation?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
    auto Update=[&](const TCHAR* Name,EProcessCommand C,double V,bool Show,const FString& Target)
    {
        auto* B=Cast<UButton>(W->GetWidgetFromName(Name));if(!B)return;
        B->SetVisibility(Show&&!Observation?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
        const FString Reason=State.UnavailableReason(Target,C,V,Now);
        bool Pending=false;for(const auto& Pair:State.History)if(Pair.Value.IsPending()&&Pair.Value.EquipmentId==Target&&Pair.Value.Command==C&&Pair.Value.Value==V)Pending=true;
        // Closing and stop remain reachable even while an opening is in flight.
        B->SetIsEnabled(Show&&!Observation&&Reason.IsEmpty()&&!Pending);B->SetToolTipText(FText::FromString(Pending?TEXT("같은 요청의 적용 확인 중"):Reason));
    };
    Update(TEXT("ValveOpen"),EProcessCommand::Valve,1,!Mixer&&!Id.IsEmpty(),Id);
    if(auto* B=Cast<UButton>(W->GetWidgetFromName(TEXT("ValveOpen"))))
        ButtonLabel(B,Id==TEXT("weigher")&&State.Snapshot.FeedPreparation.Enabled?TEXT("공급 완료 확인 후 열기"):TEXT("밸브 열기"));
    Update(TEXT("ValveClose"),EProcessCommand::Valve,0,!Mixer&&!Id.IsEmpty(),Id);
    Update(TEXT("MixerStart"),EProcessCommand::MixerEnabled,1,Mixer,Id);
    Update(TEXT("MixerStop"),EProcessCommand::MixerEnabled,0,Mixer,Id);
    Update(TEXT("SetRPM"),EProcessCommand::MixerRPM,State.Snapshot.RequestedRPM,Mixer,Id);
    Update(TEXT("ManualRequest"),EProcessCommand::StopAll,0,false,TEXT("plant"));
    Update(TEXT("GlobalStop"),EProcessCommand::StopAll,0,true,TEXT("plant"));
    Update(TEXT("ManualResume"),EProcessCommand::Resume,1,true,TEXT("plant"));
    Update(TEXT("ManualPause"),EProcessCommand::Pause,0,true,TEXT("plant"));
    if(auto* B=Cast<UButton>(W->GetWidgetFromName(TEXT("GlobalStop"))))
        ButtonLabel(B,State.StopStage==EProcessStopStage::Failed?TEXT("전체 정지 다시 시도"):TEXT("수동 조작 · 전체 정지"));
    RefreshPressureControls();RefreshPurgeControls();
    Label(W,TEXT("RPMLabel"),TEXT("목표 회전속도 · 0~30 rpm"));
    for(const TCHAR* Name:{TEXT("RPMLabel"),TEXT("RPMInput")})if(auto* C=W->GetWidgetFromName(Name))C->SetVisibility(Mixer&&!Observation?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    if(auto* Input=W->GetWidgetFromName(TEXT("RPMInput")))
        Input->SetIsEnabled(Mixer&&!Observation&&State.UnavailableReason(TEXT("mixer"),EProcessCommand::MixerRPM,State.Snapshot.RequestedRPM,Now).IsEmpty());
    for(const TCHAR* Name:{TEXT("TransferStart"),TEXT("TransferStop")})if(auto* B=Cast<UButton>(W->GetWidgetFromName(Name)))
    {B->SetIsEnabled(false);B->SetVisibility(!Mixer&&!Id.IsEmpty()&&!Observation?ESlateVisibility::Visible:ESlateVisibility::Collapsed);B->SetToolTipText(FText::FromString(State.Snapshot.PressureControlEnabled?TEXT("압력과 밸브를 각각 적용하세요. 자동 이송 시작·정지는 후속 단계입니다."):TEXT("독립적인 이송 제어 API가 없습니다. 밸브 열기는 목표량 이송 완료를 뜻하지 않습니다.")));}
    FString Message=State.UnavailableReason(Id,Mixer?EProcessCommand::MixerEnabled:EProcessCommand::Valve,1,Now);
    if(Message.IsEmpty())Message=TEXT("수동 조작 가능 · 밸브 개방은 정량 이송 완료를 뜻하지 않습니다.");
    if(!Mixer)Message+=State.Snapshot.PressureControlEnabled?TEXT("\n압력·밸브를 각각 조작하세요. 자동 이송 시작·정지는 미지원입니다."):TEXT("\n이송 시작·정지: 독립 제어 API 없음");
    const FProcessCommandRecord* Latest=nullptr;
    for(const auto& Pair:State.History)if(Pair.Value.Connection==State.Connection&&(Pair.Value.EquipmentId==Id||Pair.Value.EquipmentId==TEXT("plant")||(Pair.Value.Command==EProcessCommand::Pressure&&PlantControl::SamePressureTarget(Pair.Value.EquipmentId,Id)))&&(!Latest||Pair.Value.AcceptedAt>Latest->AcceptedAt))Latest=&Pair.Value;
    if(Latest)Message=FProcessControlMachine::StatusText(Latest->Status)+TEXT(" · ")+Latest->Message+TEXT("\n")+Message;
    Label(W,TEXT("Disconnected"),Message);
    FString GlobalMessage=TEXT("조회·화면 전환은 운전을 변경하지 않습니다. 정지 확인 후 계산 재개를 선택하세요.");
    if(State.TimeoutPolicy==EProcessTimeoutPolicy::Unspecified)GlobalMessage=TEXT("닫힘 실패 시 처리 정책을 확인한 뒤 수동 조작을 활성화할 수 있습니다.");
    else if(State.IsStopping())GlobalMessage=TEXT("미확인: ")+State.UnconfirmedEquipment();
    else if(State.StopStage==EProcessStopStage::Failed)
        GlobalMessage=TEXT("미확인: ")+(State.StopFailureEquipment.IsEmpty()?State.UnconfirmedEquipment():State.StopFailureEquipment);
    else if(State.ManualGranted&&State.Snapshot.Execution==EPlantExecutionState::Paused)
        GlobalMessage=TEXT("개방·혼합 시작 전 ‘수동 운전 · 계산 재개’를 선택하세요. 닫기 요청은 전체 정지를 확인합니다.");
    Label(W,TEXT("Help"),State.StopDescription()+TEXT("\n")+GlobalMessage);
    Label(W,TEXT("DiagramNote"),ProcessPlantBinding::IsLowPoly(State.Snapshot.SourceActorPath)?
        TEXT("전체 정지는 압력 설정 0·밸브 8개 닫힘·계산 적용 RPM 0·작업자 정지를 확인합니다.\n모델 질량·수동 이송 상태를 관찰합니다. 시험 기준이며 자동 밸브 제어·목표량 차단은 수행하지 않습니다."):
        TEXT("전체 정지는 제어 가능한 밸브·혼합·계산 정지를 확인합니다.\n독립 압력 차단·자동 이송은 미지원입니다. 수동 이송 판정은 모델 질량·시험 기준입니다."));
    if(auto* Help=W->GetWidgetFromName(TEXT("Help")))Help->SetToolTipText(FText::FromString(FString::Printf(
        TEXT("자동 시퀀스는 연결되어 있지 않습니다. 닫기 요청은 모델의 공급·이송 유량 요청을 0으로 만듭니다. 압력 지원 레벨에서는 모든 압력 목표 0의 적용도 확인합니다. 외부 장비 정지·실제 감압·분진 안전 판정은 제공하지 않습니다.\n수동 일시정지와 일시정지 중 닫기는 전체 정지 절차입니다. 시간 초과 시 계산 동결 응답까지 확인하며 실패 후 명시적 재시도가 필요합니다.\n시험용 제한: 명령·동결 응답 %.1f초 / 전체 정지 %.1f초 (실제 경과 시간)\n%s"),State.CommandTimeoutSeconds,State.StopTimeoutSeconds,*State.UnconfirmedEquipment())));
    if(Latest)if(auto* T=W->GetWidgetFromName(TEXT("Disconnected")))T->SetToolTipText(FText::FromString(FString::Printf(TEXT("요청 %s\n초기화 세대 %lld · 연결 %lld · 작업자 확인 번호 %lld\n요청값 %.9g"),*Latest->RequestId.ToString(),Latest->Epoch,Latest->Connection,Latest->Revision,Latest->Value)));
    if(Observation)Label(W,TEXT("ThreeDNotice"),FString::Printf(TEXT("3D 관찰 전용 · %s\n해석 시간 %.4f초\n장비 조작은 공정도에서만 가능합니다.\n우클릭: 회전 · 우클릭+WASD/QE: 이동 · 휠: 확대/축소"),*State.StopDescription(),State.Snapshot.PhysicalSeconds));
}
